#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>
#include <cos_component.h>
#include <cos_kernel_api.h>
#include <static_slab.h>
#include <sched.h>
#include <contigmem.h>
#include <shm_bm.h>
#include <vmrt.h>
#include <instr_emul.h>
#include <acrn_common.h>

#include <vlapic.h>
#include <vioapic.h>
#include <sync_lock.h>
#include <cos_time.h>
#include <sync_sem.h>
#include <arpa/inet.h>
#include <netshmem.h>
#include <nic_netio_shmem.h>
#include <devices/vpci/virtio_net_io.h>


INCBIN(vmlinux, "guest/vmlinux.img")
INCBIN(bios, "guest/guest.img")

/* Only one VM component globally managed by this VMM */
static struct vmrt_vm_comp *g_vm;
static u8_t g_num_vcpus = 1;  

#define VM_MAX_COMPS (2)
#define GUEST_MEM_SZ (310*1024*1024)

SS_STATIC_SLAB(vm_comp, struct vmrt_vm_comp, VM_MAX_COMPS);
SS_STATIC_SLAB(vm_lapic, struct acrn_vlapic, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);
SS_STATIC_SLAB(vcpu_inst_ctxt, struct instr_emul_ctxt, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);
SS_STATIC_SLAB(vm_io_apic, struct acrn_vioapics, VM_MAX_COMPS);
SS_STATIC_SLAB(vcpu_mmio_req, struct acrn_mmio_request, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);

struct sync_lock vm_boot_lock;

void 
pause_handler(struct vmrt_vm_vcpu *vcpu)
{
	sched_thd_yield();
	GOTO_NEXT_INST(vcpu->shared_region);
}

void 
hlt_handler(struct vmrt_vm_vcpu *vcpu)
{
	sched_thd_yield();
	GOTO_NEXT_INST(vcpu->shared_region);
}

void
mmio_init(struct vmrt_vm_vcpu *vcpu)
{
	vcpu->mmio_request = ss_vcpu_mmio_req_alloc();
	assert(vcpu->mmio_request);

	ss_vcpu_mmio_req_activate(vcpu->mmio_request);
}

void
ioapic_init(struct vmrt_vm_comp *vm)
{
	vm->ioapic = ss_vm_io_apic_alloc();
	assert(vm->ioapic);

	vioapic_init(vm);
	ss_vm_io_apic_activate(vm->ioapic);
}

void
iinst_ctxt_init(struct vmrt_vm_vcpu *vcpu)
{
	vcpu->inst_ctxt = ss_vcpu_inst_ctxt_alloc();
	assert(vcpu->inst_ctxt);

	ss_vcpu_inst_ctxt_activate(vcpu->inst_ctxt);
}

void
lapic_init(struct vmrt_vm_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = ss_vm_lapic_alloc();
	assert(vlapic);

	/* A vcpu can only have one vlapic */
	assert(vcpu->vlapic == NULL);

	vcpu->vlapic = vlapic;
	vlapic->apic_page = vcpu->lapic_page;
	vlapic->vcpu = vcpu;

	/* Status: enable APIC and vector be 0xFF */
	vlapic_reset(vlapic);

	ss_vm_lapic_activate(vlapic);
	return;
}

struct vmrt_vm_comp *
vm_comp_create(void)
{
	u64_t guest_mem_sz = GUEST_MEM_SZ;
	u64_t num_vcpu = g_num_vcpus;
	void *start;
	void *end;
	cbuf_t shm_id;
	void  *mem, *vm_mem;
	size_t sz;

	struct vmrt_vm_comp *vm = ss_vm_comp_alloc();
	assert(vm);
	
	vmrt_vm_create(vm, "vmlinux-5.15", num_vcpu, guest_mem_sz);

	/* Allocate memory for the VM */
	shm_id	= memmgr_shared_page_allocn_aligned(guest_mem_sz / PAGE_SIZE_4K, PAGE_SIZE_4K, (vaddr_t *)&mem);
	/* Make the memory accessible to VM */
	int ret = memmgr_shared_page_map_aligned_in_vm(shm_id, PAGE_SIZE_4K, (vaddr_t *)&vm_mem, vm->comp_id);
	vmrt_vm_mem_init(vm, mem);
	printc("created VM with %u cpus, memory size: %luMB, at host vaddr: %p\n", vm->num_vcpu, vm->guest_mem_sz/1024/1024, vm->guest_addr);

	ss_vm_comp_activate(vm);

	start = &incbin_bios_start;
	end = &incbin_bios_end;
	sz = end - start + 1;

	printc("BIOS image start: %p, end: %p, size: %lu(%luKB)\n", start, end, sz, sz/1024);
	vmrt_vm_data_copy_to(vm, start, sz, PAGE_SIZE_4K);
	
	start = &incbin_vmlinux_start;
	end = &incbin_vmlinux_end;
	sz = end - start + 1;

	printc("Guest Linux image start: %p, end: %p, size: %lu(%luMB)\n", start, end, sz, sz/1024/1024);
	#define GUEST_IMAGE_ADDR 0x100000
	vmrt_vm_data_copy_to(vm, start, sz, GUEST_IMAGE_ADDR);

	ioapic_init(vm);

	printc("Guest(%s) image has been loaded into the VM component\n", vm->name);

	return vm;
}

/* Weak usage of nicmgr interface */
void __attribute__((weak)) nic_netio_shmem_map(cbuf_t shm_id);

void
cos_init(void)
{
	struct vmrt_vm_vcpu *vcpu;
	g_vm = vm_comp_create();
	printc("VMM: Created VM with %d vCPUs: comp_id=%lu, %p\n", g_num_vcpus, g_vm->comp_id, g_vm);

	if (nic_netio_shmem_map) {
		/* Create TX thread for transmitting packets to NIC */
		g_vm->tx_thd = sched_thd_create(virtio_tx_task, NULL);
		printc("VMM: Created TX thread, tid=%lu\n", g_vm->tx_thd);
		/* Create RX thread for receiving packets from NIC */
		g_vm->rx_thd = sched_thd_create(virtio_rx_task, NULL);
		printc("VMM: Created RX thread, tid=%lu\n", g_vm->rx_thd);
	} else {
		printc("VMM: Networking disabled: nicmgr interface not found\n");
	}
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	struct vmrt_vm_vcpu *vcpu;

	if (cid == 0) {
		printc("VMM: Initializing vCPU 0 on core %d\n", cid);
		vmrt_vm_vcpu_init(g_vm, 0);
		vcpu = vmrt_get_vcpu(g_vm, 0);
		printc("VMM: vCPU 0 initialized (tid=%lu)\n", vcpu->tid);
		lapic_init(vcpu);
		iinst_ctxt_init(vcpu);
		mmio_init(vcpu);
		printc("VMM: vCPU 0 lapic, inst_ctxt, mmio initialized\n");
	}

	return;
}

void
parallel_main(coreid_t cid)
{
	struct vmrt_vm_vcpu *vcpu;
	if (cid == 0 && nic_netio_shmem_map) {
		sched_thd_param_set(g_vm->rx_thd, sched_param_pack(SCHEDP_PRIO, 31));
		sched_thd_param_set(g_vm->tx_thd, sched_param_pack(SCHEDP_PRIO, 31));
	}

	if (cid == 0) {
		printc("VMM: Starting vCPU 0 on core %d\n", cid);
		vcpu = vmrt_get_vcpu(g_vm, 0);
		printc("VMM: Core %d started vCPU %d (tid=%lu)\n", cid, cid, vcpu->tid);
		vmrt_vm_vcpu_start(vcpu);
	}
	sched_thd_block(0);
	/* Should not be here, or there is a bug in the scheduler! */
	assert(0);
}
