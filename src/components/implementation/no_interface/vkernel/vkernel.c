#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "vk_types.h"

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

struct vms_info {
	struct cos_compinfo cinfo;
	captblcap_t ct;
	pgtblcap_t pt, utpt;
	compcap_t cc;
	thdcap_t initthd, exitthd;
	thdid_t inittid;
	tcap_t inittcap;
	arcvcap_t initrcv;
} vmx_info[VM_COUNT];

struct vkernel_info {
	struct cos_compinfo cinfo;

	thdcap_t termthd;
	asndcap_t vminitasnd[VM_COUNT];
} vk_info;

extern vaddr_t cos_upcall_entry;
extern void vm_init(void *);

unsigned int ready_vms = VM_COUNT;
struct cos_compinfo *vk_cinfo = (struct cos_compinfo *)&vk_info.cinfo;

void
vk_terminate(void *d)
{
	BUG();
}

void
vm_exit(void *d)
{
	printc("%d: EXIT\n", (int)d);
	ready_vms --;
	vmx_info[(int)d].initthd = 0;	

	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

void
scheduler(void) 
{
	static unsigned int i;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	int index;

	while (ready_vms) {
		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
		index = i++ % VM_COUNT;
		
		if (vmx_info[index].initthd) {
			assert(vk_info.vminitasnd[index]);
			cos_asnd(vk_info.vminitasnd[index]);
		}
	}
}


void
cos_init(void)
{
	int id;

	printc("vkernel: START\n");
	assert(VM_COUNT >= 2);

	cos_meminfo_init(&vk_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(vk_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, vk_cinfo);

	vk_info.termthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vk_terminate, NULL);
	assert(vk_info.termthd);

	for (id = 0 ; id < VM_COUNT ; id ++) {
		struct cos_compinfo *vm_cinfo = &vmx_info[id].cinfo;
		struct vms_info *vm_info = &vmx_info[id];
		vaddr_t vm_range, addr;
		int ret;

		printc("vkernel: VM%d Init START\n", id);
		printc("\tForking VM\n");
		vm_info->exitthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vm_exit, (void *)id);
		assert(vm_info->exitthd);
		
		vm_info->ct = cos_captbl_alloc(vk_cinfo);
		assert(vm_info->ct);

		vm_info->pt = cos_pgtbl_alloc(vk_cinfo);
		assert(vm_info->pt);

		vm_info->utpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vm_info->utpt);

		vm_info->cc = cos_comp_alloc(vk_cinfo, vm_info->ct, vm_info->pt, (vaddr_t)&cos_upcall_entry);
		assert(vm_info->cc);

		cos_meminfo_init(&vm_cinfo->mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, vm_info->utpt);
		cos_compinfo_init(vm_cinfo, vm_info->pt, vm_info->ct, vm_info->cc,
				(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, vk_cinfo);

		vm_info->initthd = cos_thd_alloc(vk_cinfo, vm_cinfo->comp_cap, vm_init, (void *)id);
		assert(vm_info->initthd);
		vm_info->inittid = (thdid_t)cos_introspect(vk_cinfo, vm_info->initthd, 9);
		printc("\tInit thread= cap:%x tid:%x\n", (unsigned int)vm_info->initthd, (unsigned int)vm_info->inittid);

		printc("\tCopying pgtbl, captbl, component capabilities\n");
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_CT, vk_cinfo, vm_info->ct);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_PT, vk_cinfo, vm_info->pt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_UNTYPED_PT, vk_cinfo, vm_info->utpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_COMP, vk_cinfo, vm_info->cc);
		assert(ret == 0);

		printc("\tCreating and copying required initial capabilities\n");
		/*
		 * TODO: Multi-core support to create INITIAL Capabilities per core
		 */
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE, vk_cinfo, vm_info->initthd);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE, vk_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_SELF_EXITTHD_BASE, vk_cinfo, vm_info->exitthd);
		assert(ret == 0);
		
		vm_info->inittcap = cos_tcap_alloc(vk_cinfo, TCAP_PRIO_MAX);
		assert(vm_info->inittcap);

		vm_info->initrcv = cos_arcv_alloc(vk_cinfo, vm_info->initthd, vm_info->inittcap, vk_cinfo->comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(vm_info->initrcv);

		ret = cos_tcap_transfer(vm_info->initrcv, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE, vk_cinfo, vm_info->inittcap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITRCV_BASE, vk_cinfo, vm_info->initrcv);
		assert(ret == 0);

		/*
		 * Create send end-point in VKernel to each VM's INITRCV end-point
		 */
		vk_info.vminitasnd[id] = cos_asnd_alloc(vk_cinfo, vm_info->initrcv, vk_cinfo->captbl_cap);
		assert(vk_info.vminitasnd[id]);

		/*
		 * Create and copy booter comp virtual memory to each VM
		 */
		vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
		assert(vm_range > 0);
		printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
		for (addr = 0 ; addr < vm_range ; addr += PAGE_SIZE) {
			vaddr_t spg = (vaddr_t)cos_page_bump_alloc(vk_cinfo), dpg;
			assert(spg);
			
			memcpy((void *)spg, (void *)(BOOT_MEM_VM_BASE + addr), PAGE_SIZE);
			
			dpg = cos_mem_alias(vm_cinfo, vk_cinfo, spg);
			assert(dpg);
		}

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE);
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE);

		printc("vkernel: VM%d Init END\n", id);
	}

	printc("Starting Scheduler\n");
	cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC, BOOT_CAPTBL_SELF_INITRCV_BASE);
	printc("\t%d cycles per microsecond\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));
	printc("------------------[ VKernel & VMs init complete ]------------------\n");

	scheduler();

	cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);

	printc("vkernel: END\n");
	cos_thd_switch(vk_info.termthd);

	printc("DEAD END\n");

	return;
}
