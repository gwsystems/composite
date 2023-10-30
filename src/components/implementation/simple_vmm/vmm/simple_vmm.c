#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>
#include <cos_component.h>
#include <cos_kernel_api.h>
#include <static_slab.h>
// #include <perfdata.h>
// #include <cos_ubench.h>
#include <sched.h>
#include <contigmem.h>
#include <shm_bm.h>
#include <vmrt.h>

#include <vlapic.h>

INCBIN(vmlinux, "guest/vmlinux.img")
INCBIN(bios, "guest/guest.img")


/* Currently only have one VM component globally managed by this VMM */
static struct vmrt_vm_comp *g_vm;

#define VM_MAX_COMPS (2)

SS_STATIC_SLAB(vm_comp, struct vmrt_vm_comp, VM_MAX_COMPS);
SS_STATIC_SLAB(vm_lapic, struct acrn_vlapic, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);

void
init_lapic(struct vmrt_vm_vcpu *vcpu)
{
	struct lapic_regs *lapic;
	struct acrn_vlapic *vlapic = ss_vm_lapic_alloc();
	assert(vlapic);

	/* A vcpu can only have one vlapic */
	assert(vcpu->vlapic == NULL);

	vcpu->vlapic = vlapic;
	vlapic->apic_page = vcpu->lapic_page;
	vlapic->vcpu = vcpu;

	lapic = vlapic->apic_page;

	/* Status: enable APIC and vector be 0xFF */
	lapic->svr.v = 0x1FF;
	lapic->version.v = 0x50015;
	lapic->id.v = 0x3000000;

	lapic->dfr.v = 0xFFFFFFFFU;
	lapic->icr_timer.v = 0U;
	lapic->dcr_timer.v = 0U;

	vlapic->svr_last = lapic->svr.v;
	vlapic->isrv = 0U;

	ss_vm_lapic_activate(vlapic);
	return;
}

struct vmrt_vm_comp *
vm_comp_create(void)
{
	u64_t guest_mem_sz = 64*1024*1024;
	u64_t num_vpu = 1;
	void *start;
	void *end;
	cbuf_t shm_id;
	void  *mem, *vm_mem;
	size_t sz;

	struct vmrt_vm_comp *vm = ss_vm_comp_alloc();
	assert(vm);
	
	vmrt_vm_create(vm, "vmlinux-5.15", num_vpu, guest_mem_sz);

	/* Allocate memory for the VM */
	shm_id	= contigmem_shared_alloc_aligned(guest_mem_sz / PAGE_SIZE_4K, PAGE_SIZE_4K, (vaddr_t *)&mem);
	/* Make the memory accessible to VM */
	memmgr_shared_page_map_aligned_in_vm(shm_id, PAGE_SIZE_4K, (vaddr_t *)&vm_mem, vm->comp_id);
	vmrt_vm_mem_init(vm, mem);
	printc("created VM with %u cpus, memory size: %luMB, at host vaddr: %p\n", vm->num_vpu, vm->guest_mem_sz/1024/1024, vm->guest_addr);

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

	printc("Guest(%s) image has been loaded into the VM component\n", vm->name);

	return vm;
}

void
cos_init(void)
{
	g_vm = vm_comp_create();
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	struct vmrt_vm_vcpu *vcpu;

	vmrt_vm_vcpu_init(g_vm, cid);
	vcpu = vmrt_get_vcpu(g_vm, cid);

	init_lapic(vcpu);
	return;
}

void
parallel_main(coreid_t cid)
{
	struct vmrt_vm_vcpu *vcpu;
	vcpu = vmrt_get_vcpu(g_vm, cid);

	vmrt_vm_vcpu_start(vcpu);

	while (1)
	{
		sched_thd_block(0);
		/* Should not be here, or there is a bug in the scheduler! */
		assert(0);
	}
}
