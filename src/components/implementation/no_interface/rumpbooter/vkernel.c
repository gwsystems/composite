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
extern void   *__inv_vkernel_hypercall(int a, int b, int c, int d);
unsigned int cycs_per_usec;

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
	int id;

	if (is_booter == 0) {
		printc("vkernel: START child\n");
		vm_init(NULL);
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

	vk_info.sinv = cos_sinv_alloc(vk_cinfo, vk_cinfo->comp_cap, (vaddr_t)__inv_vkernel_hypercall);
	assert(vk_info.sinv);

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs_per_usec);

	spinlib_calib();

	sl_init(PARENT_PERIOD_US);

	for (id = 0 ; id < VM_COUNT ; id ++) {
		struct cos_compinfo *vm_cinfo = cos_compinfo_get(&(vmx_info[id].dci));
		struct vms_info     *vm_info = &vmx_info[id];
		vaddr_t              vm_range, addr;
		int                  ret, i;

		printc("vkernel: %s %d Init START\n", id < APP_START_ID ? "VM" : "APP", id);
		vm_info->id = id;

		/*
		 * Array of all components for use in the shdmem api.
		 * shdmem api is handled by this booting component.
		 * TODO, since this booting component is also scheduler, is this a problem?
		 */
		assert(vm_cinfo);
		printc("vm_cinfo: %p for id: %d\n", vm_cinfo, vm_info->id);
		shm_infos[id].cinfo = vm_cinfo;
		shm_infos[id].shm_frontier = VK_VM_SHM_BASE; /* 2Gb */

		vk_vm_create(vm_info, &vk_info);

		vk_vm_sinvs_alloc(vm_info, &vk_info);

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE(id));
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE(id));

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

		vk_vm_sched_init(vm_info);
		printc("vkernel: %s %d Init END\n", id < APP_START_ID ? "VM" : "APP", id);

		if (id == VM_COUNT - 1) {
			vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
			assert(vm_range > 0);

			/*
			 * Create and copy booter comp virtual memory to each VM
			 * Copy DOM0 only after all VMs are initialized
			 */

			for (i = id; i >= 0; i --) {
				printc("Mapping in Booter component's virtual memory (range:%lu) to vmid %d\n", vm_range, i);
				vk_vm_virtmem_alloc(&vmx_info[i], &vk_info, BOOT_MEM_VM_BASE, vm_range);
			}
		}


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
