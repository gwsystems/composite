#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <vkern_api.h>
#include <vk_api.h>
#include <sinv_calls.h>
#include <shdmem.h>
#include "cos_sync.h"
#include "vk_types.h"

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

extern thdcap_t cos_cur;
extern void vm_init(void *);
extern void kernel_init(void *);
extern vaddr_t cos_upcall_entry;
uint64_t t_vm_cycs  = 0;
uint64_t t_dom_cycs = 0;

unsigned int cycs_per_usec;

thdcap_t vm_main_thd;
thdid_t  vm_main_thdid;

struct vkernel_info vk_info;
struct cos_compinfo *vk_cinfo = (struct cos_compinfo *)&vk_info.cinfo;

struct vms_info user_info;
struct cos_compinfo *user_cinfo = (struct cos_compinfo *)&user_info.cinfo;

struct vms_info kernel_info;
struct cos_compinfo *kernel_cinfo = (struct cos_compinfo *)&kernel_info.cinfo;

/* Unused Zombie Stubs */
void
vm_io_fn(void *unused)
{ /* Unimplemented */ }

void
dom0_io_fn(void *unused)
{ /* Unimplemented */ }

void
vm_exit(void *unused)
{ /* Unimplemented */ }


void
cos_init(void)
{
	printc("Hypervisor:booter component START\n");

	int i = 0, id = 0, cycs;
	int page_range = 0;


	/* Initialize kernel component */
	cos_meminfo_init(&vk_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(vk_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, vk_cinfo);

	cos_compinfo_init(&vk_info.shm_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)VK_VM_SHM_BASE, BOOT_CAPTBL_FREE, vk_cinfo);


	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE)));
	printc("\t%d cycles per microsecond\n", cycs);
	cycs_per_usec = (unsigned int)cycs;

	/*
	 * NOTE:
	 * Fork kernel and user components
	 * kernel component has id = 0
	 * user component has id = 1
	 */
	printc("user_cinfo: %p, kernel_cinfo: %p\n", user_cinfo, kernel_cinfo);

	/* Save temp struct cos_compinfo for copying kernel component virtual memory below */
	for (id = 0 ; id < 2 ; id++) {
		struct vms_info *vm_info;
		struct cos_compinfo *vm_cinfo;
                if (id == 0) {
			vm_info = &kernel_info;
			vm_cinfo = kernel_cinfo;
		} else if (id == 1) {
			vm_info = &user_info;
			vm_cinfo = user_cinfo;
		}
		vaddr_t vm_range, addr;
		pgtblcap_t vmpt, vmutpt;
		captblcap_t vmct;
		compcap_t vmcc;
		int ret;

		printc("booter: VM%d Init START\n", id);
		vm_info->id = id;

		/* Array of all components for use in the shdmem api that is handled by this booter component */
		assert(vm_cinfo);
                printc("vm_cinfo for spdid %d: %p\n", vm_info->id, vm_cinfo);
		shm_infos[id].cinfo = vm_cinfo;
		shm_infos[id].shm_frontier = 0x80000000; /* 2Gb */

		printc("\tForking booter\n");
		vmct = cos_captbl_alloc(vk_cinfo);
		assert(vmct);

		vmpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vmpt);

		vmutpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vmutpt);

		vmcc = cos_comp_alloc(vk_cinfo, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
		assert(vmcc);

		cos_meminfo_init(&vm_cinfo->mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, vmutpt);
		cos_compinfo_init(vm_cinfo, vmpt, vmct, vmcc,
			(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, vk_cinfo);
		cos_compinfo_init(&vm_info->shm_cinfo, vmpt, vmct, vmcc,
			(vaddr_t)VK_VM_SHM_BASE, VM_CAPTBL_FREE, vk_cinfo);

		printc("\tCopying pgtbl, captbl, component capabilities\n");
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_CT, vk_cinfo, vmct);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_PT, vk_cinfo, vmpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_UNTYPED_PT, vk_cinfo, vmutpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_COMP, vk_cinfo, vmcc);
		assert(ret == 0);
		printc("\tDone copying capabilities\n");

		printc("\tInitializing capabilities\n");
		/* We have different initialization functions for kernel and user components */
		/* FIXME change vk_initcaps_init to user and rk to kernel */
		if (id == 0) {
			printc("kernel_info: %p, kernel_cinfo: %p, &kernel_info.cinfo: %p\n", &kernel_info, kernel_cinfo, &kernel_info.cinfo);
			printc("vm_info: %p, vm_cinfo: %p, &vm_info->cinfo: %p\n", vm_info, vm_cinfo, &vm_info->cinfo);
			rk_initcaps_init(vm_info, &vk_info);
		} else if (id > 0)  {
			vk_initcaps_init(vm_info, &vk_info);
			printc("\tCoppying in vm_main_thd capability into Kernel component\n");
			ret = cos_cap_cpy_at(kernel_cinfo, BOOT_CAPTBL_USERSPACE_THD,
					     vm_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE);
			assert(ret == 0);
		}
		printc("\tDone Initializing capabilities\n");

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE);
		/* BOOT_MEM_KM_BASE = PGD_SIZE = 1 << 22 = 0x00400000 = 4MB */
		/* VM_UNTYPED_SIZE = 1<<27 which is 124 MB */
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE);

		/* User component must be initializing and kernel component must be done, wait for id > 0 */
		if (id > 0) {
			sinvcap_t sinv;
			printc("\tSetting up sinv capability from user component to kernel component\n");
			sinv = cos_sinv_alloc(vk_cinfo, kernel_cinfo->comp_cap, (vaddr_t)__inv_test_fs);
			assert(sinv > 0);
			/* Copy into user capability table at a known location */
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_BASE, vk_cinfo, sinv);
			assert(ret == 0);

			/* Testing shared memory from user component to kernel component  */
			sinv = cos_sinv_alloc(vk_cinfo, kernel_cinfo->comp_cap, (vaddr_t)__inv_test_shdmem);
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_TEST, vk_cinfo, sinv);
			assert(ret == 0);

			printc("\tSetting up sinv capabilities from kernel and user component to booter component\n");
			/* TODO: change enum name from VM0_... either VM or nothing */
			/* Shmem Syncronous Invocations */
			sinv = cos_sinv_alloc(vk_cinfo, vk_cinfo->comp_cap, (vaddr_t)__inv_shdmem_get_vaddr);
			assert(sinv > 0);
			ret = cos_cap_cpy_at(kernel_cinfo, VM0_CAPTBL_SELF_IOSINV_VADDR_GET, vk_cinfo, sinv);
			assert(ret == 0);
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_VADDR_GET, vk_cinfo, sinv);
			assert(ret == 0);

			sinv = cos_sinv_alloc(vk_cinfo, vk_cinfo->comp_cap, (vaddr_t)__inv_shdmem_allocate);
			assert(sinv > 0);
			ret = cos_cap_cpy_at(kernel_cinfo, VM0_CAPTBL_SELF_IOSINV_ALLOC, vk_cinfo, sinv);
			assert(ret == 0);
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_ALLOC, vk_cinfo, sinv);
			assert(ret == 0);

			sinv = cos_sinv_alloc(vk_cinfo, vk_cinfo->comp_cap, (vaddr_t)__inv_shdmem_deallocate);
			assert(sinv > 0);
			ret = cos_cap_cpy_at(kernel_cinfo, VM0_CAPTBL_SELF_IOSINV_DEALLOC, vk_cinfo, sinv);
			assert(ret == 0);
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_DEALLOC, vk_cinfo, sinv);
			assert(ret == 0);

			sinv = cos_sinv_alloc(vk_cinfo, vk_cinfo->comp_cap, (vaddr_t)__inv_shdmem_map);
			assert(sinv > 0);
			ret = cos_cap_cpy_at(kernel_cinfo, VM0_CAPTBL_SELF_IOSINV_MAP, vk_cinfo, sinv);
			assert(ret == 0);
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_MAP, vk_cinfo, sinv);
			assert(ret == 0);

			printc("\tDone setting up sinvs\n");

			/* Create and copy booter comp virtual memory to each VM */
			vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
			assert(vm_range > 0);
			printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
			vk_virtmem_alloc(vm_info, &vk_info, BOOT_MEM_VM_BASE, vm_range);

			/* Copy Kernel component after userspace component is initialized */
			if (id == 1) {
				vk_virtmem_alloc(&kernel_info, &vk_info, BOOT_MEM_VM_BASE, vm_range);
			}
		}

		printc("booter: VM%d Init END\n", id);
		vm_info->state = VM_RUNNING;
	}

	printc("------------------[ Booter & VMs init complete ]------------------\n");

	printc("\nRechecking the untyped memory in the booter...\n");
	printc("booter's untyped_frontier is: %p, untyped_ptr is: %p\n", vk_cinfo->mi.untyped_frontier, vk_cinfo->mi.untyped_ptr);
	printc("kernel's untyped_frontier is: %p, untyped_ptr is: %p\n", kernel_cinfo->mi.untyped_frontier, kernel_cinfo->mi.untyped_ptr);

	printc("\n------------------[ Starting Kernel ]--------------------\n");

	cos_thd_switch(kernel_info.initthd);

	printc("BACK IN BOOTER COMPONENT, DEAD END!\n");

	return;
}
