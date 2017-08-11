#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "vk_types.h"
#include "vk_api.h"

#undef assert
#define assert(node)                                       \
	do {                                               \
		if (unlikely(!(node))) {                   \
			debug_print("assert error in @ "); \
			*((int *)0) = 0;                   \
		}                                          \
	} while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG()                          \
	do {                           \
		debug_print("BUG @ "); \
		*((int *)0) = 0;       \
	} while (0);
#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)

extern vaddr_t cos_upcall_entry;
extern void    vm_init(void *);

struct vms_info      vmx_info[VM_COUNT];
struct dom0_io_info  dom0ioinfo;
struct vm_io_info    vmioinfo[VM_COUNT - 1];
struct vkernel_info  vk_info;
unsigned int         ready_vms = VM_COUNT;
struct cos_compinfo *vk_cinfo  = (struct cos_compinfo *)&vk_info.cinfo;

void
vk_terminate(void *d)
{
	SPIN();
}

void
vm_exit(void *d)
{
	printc("%d: EXIT\n", (int)d);
	ready_vms--;
	vmx_info[(int)d].state = VM_EXITED;

	while (1)
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

void
scheduler(void)
{
	static unsigned int i;
	thdid_t             tid;
	int                 blocked;
	cycles_t            cycles;
	int                 index;

	while (ready_vms) {
		index = i++ % VM_COUNT;

		if (vmx_info[index].state == VM_RUNNING) {
			assert(vk_info.vminitasnd[index]);

			if (cos_tcap_delegate(vk_info.vminitasnd[index],
			                      BOOT_CAPTBL_SELF_INITTCAP_BASE,
			                      VM_BUDGET_FIXED,
			                      VM_PRIO_FIXED,
			                      TCAP_DELEG_YIELD))
				assert(0);
		}

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, 0, NULL, &tid, &blocked, &cycles))
			;
	}
}


void
cos_init(void)
{
	int id, cycs;

	printc("vkernel: START\n");
	assert(VM_COUNT >= 2);

	cos_meminfo_init(&vk_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(vk_cinfo,
	                  BOOT_CAPTBL_SELF_PT,
	                  BOOT_CAPTBL_SELF_CT,
	                  BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(),
	                  BOOT_CAPTBL_FREE,
	                  vk_cinfo);
	/*
	 * TODO: If there is any captbl modification, this could mess up a bit.
	 *       Care to be taken not to use this for captbl mod api
	 *       Or use some offset into the future in CAPTBL_FREE
	 */
	cos_compinfo_init(&vk_info.shm_cinfo,
	                  BOOT_CAPTBL_SELF_PT,
	                  BOOT_CAPTBL_SELF_CT,
	                  BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)VK_VM_SHM_BASE,
	                  BOOT_CAPTBL_FREE,
	                  vk_cinfo);

	vk_info.termthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vk_terminate, NULL);
	assert(vk_info.termthd);

	cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs);

	for (id = 0; id < VM_COUNT; id++) {
		struct cos_compinfo *vm_cinfo = &vmx_info[id].cinfo;
		struct vms_info *    vm_info  = &vmx_info[id];
		vaddr_t              vm_range, addr;
		pgtblcap_t           vmpt, vmutpt;
		captblcap_t          vmct;
		compcap_t            vmcc;
		int                  ret;

		printc("vkernel: VM%d Init START\n", id);
		vm_info->id = id;

		printc("\tForking VM\n");
		vmct = cos_captbl_alloc(vk_cinfo);
		assert(vmct);

		vmpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vmpt);

		vmutpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vmutpt);

		vmcc = cos_comp_alloc(vk_cinfo, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
		assert(vmcc);

		cos_meminfo_init(&vm_cinfo->mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, vmutpt);
		cos_compinfo_init(vm_cinfo, vmpt, vmct, vmcc, (vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, vk_cinfo);
		cos_compinfo_init(
		  &vm_info->shm_cinfo, vmpt, vmct, vmcc, (vaddr_t)VK_VM_SHM_BASE, VM_CAPTBL_FREE, vk_cinfo);

		printc("\tCopying pgtbl, captbl, component capabilities\n");
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_CT, vk_cinfo, vmct);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_PT, vk_cinfo, vmpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_UNTYPED_PT, vk_cinfo, vmutpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_COMP, vk_cinfo, vmcc);
		assert(ret == 0);

		vk_initcaps_init(vm_info, &vk_info);

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE);
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE);

		if (id == 0) {
			printc("\tAllocating shared-memory (size: %lu)\n", (unsigned long)VM_SHM_ALL_SZ);
			vk_shmem_alloc(vm_info, &vk_info, VK_VM_SHM_BASE, VM_SHM_ALL_SZ);

			vm_info->dom0io = &dom0ioinfo;
		} else {
			printc("\tMapping in shared-memory (size: %lu)\n", (unsigned long)VM_SHM_SZ);
			vk_shmem_map(vm_info, &vk_info, VK_VM_SHM_BASE, VM_SHM_SZ);

			vm_info->vmio = &vmioinfo[id - 1];
		}

		if (id > 0) {
			printc("\tSetting up Cross-VM (between DOM0 and VM%d) communication capabilities\n", id);
			vk_iocaps_init(vm_info, &vmx_info[0], &vk_info);

			/*
			 * Create and copy booter comp virtual memory to each VM
			 */
			vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
			assert(vm_range > 0);
			printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
			vk_virtmem_alloc(vm_info, &vk_info, BOOT_MEM_VM_BASE, vm_range);

			/*
			 * Copy DOM0 only after all VMs are initialized
			 */
			if (id == VM_COUNT - 1) {
				vk_virtmem_alloc(&vmx_info[0], &vk_info, BOOT_MEM_VM_BASE, vm_range);
			}
		}

		printc("vkernel: VM%d Init END\n", id);
		vm_info->state = VM_RUNNING;
	}

	printc("Starting Scheduler\n");
	printc("------------------[ VKernel & VMs init complete ]------------------\n");

	scheduler();

	printc("vkernel: END\n");
	cos_thd_switch(vk_info.termthd);

	printc("vkernel: back in initial thread after switching to terminal thread. ERROR.\n");

	return;
}
