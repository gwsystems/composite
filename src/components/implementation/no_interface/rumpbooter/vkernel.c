#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <sl.h>

#include "vk_types.h"
#include "vk_structs.h"
#include "vk_api.h"
#include "spinlib.h"

#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)

extern vaddr_t cos_upcall_entry;
extern void    vm_init(void *d);
extern void    kernel_init(void *d);
extern void   *__inv_vkernel_hypercallfn(int a, int b, int c);

/* Init thread for userspace vm, needed to register within RK */
thdcap_t vm_main_thd;

struct vms_info      vmx_info[VM_COUNT];
struct dom0_io_info  dom0ioinfo;
struct vm_io_info    vmioinfo[VM_COUNT - 1];
struct vkernel_info  vk_info;
struct cos_compinfo *vk_cinfo;
unsigned int         ready_vms = VM_COUNT;

void
vk_terminate(void *d)
{
	SPIN();
}

void
cos_init(void)
{
	static int is_booter = 1;
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = cos_compinfo_get(dci);
	int id, cycs;

	if (is_booter == 0) {
		int vmid = cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_ID << 16 | cos_thdid(), 0, 0, 0);
		if (!vmid) kernel_init((void *)vmid);
		else vm_init((void *)vmid);
		SPIN();
	}
	is_booter = 0;

	printc("vkernel: START\n");
	assert(VM_COUNT >= 2);

	vk_cinfo = ci;
	cos_meminfo_init(&ci->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	/*
	 * TODO: If there is any captbl modification, this could mess up a bit.
	 *       Care to be taken not to use this for captbl mod api
	 *       Or use some offset into the future in CAPTBL_FREE
	 */
	cos_compinfo_init(&vk_info.shm_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)VK_VM_SHM_BASE, BOOT_CAPTBL_FREE, ci);

	vk_info.termthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vk_terminate, NULL);
	assert(vk_info.termthd);

	vk_info.sinv = cos_sinv_alloc(vk_cinfo, vk_cinfo->comp_cap, (vaddr_t)__inv_vkernel_hypercallfn);
	assert(vk_info.sinv);

	cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs);

	spinlib_calib();

	sl_init();

	for (id = 0 ; id < VM_COUNT ; id ++) {
		struct cos_compinfo *kernel_cinfo, *vm_cinfo = cos_compinfo_get(&(vmx_info[id].dci));
		struct vms_info     *vm_info = &vmx_info[id];
		vaddr_t              vm_range, addr;
		int                  ret;

		printc("vkernel: VM%d Init START\n", id);
		vm_info->id = id;

		/*
		 * Array of all components for use in the shdmem api.
		 * shdmem api is handled by this booting component.
		 * TODO, since this booting component is also scheduler, is this a problem?
		 */
		assert(vm_cinfo);
		printc("vm_cinfo: %p for id: %d\n", vm_cinfo, vm_info->id);
		shm_infos[id].cinfo = vm_cinfo;
		shm_infos[id].shm_frontier = 0x80000000; /* 2Gb */

		vk_vm_create(vm_info, &vk_info);

		/*
		 * Allocate syncronos invocations.
		 * Make sure we have at least 2 components.
		 */
		/* TODO add kernel_cinfo && only do this when id > 0 */
		assert(VM_COUNT > 1);
		if (id == 0) {
			/* This is our kernel component */
			kernel_cinfo = vm_cinfo;
		} else {
			assert(kernel_cinfo != vm_cinfo);
			sinv_init_all(vk_cinfo, vm_cinfo, kernel_cinfo);
		}

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE);
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE);

		if (id == 0) {
			/* TODO Look into shared memory ringbuffer, replace with my shared memory implementation */
			//printc("\tAllocating shared-memory (size: %lu)\n", (unsigned long)VM_SHM_ALL_SZ);
			//vk_vm_shmem_alloc(vm_info, &vk_info, VK_VM_SHM_BASE, VM_SHM_ALL_SZ);

			vm_info->dom0io = &dom0ioinfo;
		} else {
			/* TODO see above */
			//printc("\tMapping in shared-memory (size: %lu)\n", (unsigned long)VM_SHM_SZ);
			//vk_vm_shmem_map(vm_info, &vk_info, VK_VM_SHM_BASE, VM_SHM_SZ);

			vm_info->vmio = &vmioinfo[id - 1];
		}

		if (id > 0) {
			printc("\tSetting up Cross-VM (between DOM0 and VM%d) communication capabilities\n", id);
			vk_vm_io_init(vm_info, &vmx_info[0], &vk_info);

			/*
			 * Create and copy booter comp virtual memory to each VM
			 */
			vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
			assert(vm_range > 0);
			printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
			vk_vm_virtmem_alloc(vm_info, &vk_info, BOOT_MEM_VM_BASE, vm_range);

			/*
			 * Copy DOM0 only after all VMs are initialized
			 */
			if (id == VM_COUNT - 1) {
				vk_vm_virtmem_alloc(&vmx_info[0], &vk_info, BOOT_MEM_VM_BASE, vm_range);
			}
		}

		vk_vm_sched_init(vm_info);
		if (id == KERNEL_VM) {
			/*
			 * TODO, this is wrong, need to take the initthd for USERSPACE_VM
			 * and copy it into a location in the KERNEL_VM, then use that capability
			 * as the initthd.
			 */
			vm_main_thd = sl_thd_thdcap(vm_info->inithd);
			assert(vm_main_thd);
		}
		printc("vkernel: VM%d Init END\n", id);
	}

	printc("Starting Scheduler\n");
	printc("------------------[ VKernel & VMs init complete ]------------------\n");

	sl_sched_loop();

	printc("vkernel: END\n");
	cos_thd_switch(vk_info.termthd);

	printc("vkernel: back in initial thread after switching to terminal thread. ERROR.\n");
	assert(-1);

	return;
}
