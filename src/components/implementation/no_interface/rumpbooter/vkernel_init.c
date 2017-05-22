#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <vkern_api.h>
#include <vk_api.h>
#include <sinv_calls.h>
#include "cos_sync.h"
#include "vk_types.h"

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

extern thdcap_t cos_cur;
extern void vm_init(void *);
extern void kernel_init(void);
extern vaddr_t cos_upcall_entry;
uint64_t t_vm_cycs  = 0;
uint64_t t_dom_cycs = 0;

unsigned int cycs_per_usec;

thdcap_t vm_main_thd;
thdid_t  vm_main_thdid;

struct vkernel_info vk_info;
struct cos_compinfo *vk_cinfo = (struct cos_compinfo *)&vk_info.cinfo;

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
	struct vms_info kernel_info;
	struct cos_compinfo *kernel_cinfo = &kernel_info.cinfo;


	/* Initialize kernel component */
	cos_meminfo_init(&vk_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(vk_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, vk_cinfo);

	cos_compinfo_init(&vk_info.shm_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)VK_VM_SHM_BASE, BOOT_CAPTBL_FREE, vk_cinfo);


	printc("BOOT_CAPTBL_SELF_INITTCAP_BASE: %d\n", BOOT_CAPTBL_SELF_INITTCAP_BASE);
	printc("BOOT_CAPTBL_SELF_INITRCV_BASE: %d\n", BOOT_CAPTBL_SELF_INITRCV_BASE);
	printc("BOOT_CAPTBL_SELF_INITHW_BASE: %d\n", BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("BOOT_CAPTBL_USERSPACE_THD: %d\n", BOOT_CAPTBL_USERSPACE_THD);
	printc("BOOT_CAPTBL_LAST_CAP: %d\n", BOOT_CAPTBL_LAST_CAP);
	printc("BOOT_CAPTBL_FREE: %d\n", BOOT_CAPTBL_FREE);

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE)));
	printc("\t%d cycles per microsecond\n", cycs);
	cycs_per_usec = (unsigned int)cycs;

	/*
	 * Fork kernel and user components
	 * kernel component has id = 0
	 * user component has id = 1
	 */
	/* Save temp struct cos_compinfo for copying kernel component virtual memory below */
	for (id = 0 ; id < 2 ; id++) {
		struct vms_info vm_info;
		struct cos_compinfo *vm_cinfo = &vm_info.cinfo;
		vaddr_t vm_range, addr;
		pgtblcap_t vmpt, vmutpt;
		captblcap_t vmct;
		compcap_t vmcc;
		int ret;

		printc("booter: VM%d Init START\n", id);
		vm_info.id = id;

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
		cos_compinfo_init(&vm_info.shm_cinfo, vmpt, vmct, vmcc,
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
		if (id == 0) rk_initcaps_init(&vm_info, &vk_info);
		if (id > 0)  {
			vk_initcaps_init(&vm_info, &vk_info);
			printc("\tCoppying in vm_main_thd capability into Kernel component\n");
			ret = cos_cap_cpy_at(kernel_cinfo, BOOT_CAPTBL_USERSPACE_THD,
					     vm_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE);
			assert(ret == 0);
		}
		printc("\tDone Initializing capabilities\n");

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE);
		printc("\tBOOT_MEM_KM_BASE: %d\n", BOOT_MEM_KM_BASE);
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE);

		if (id == 0) kernel_info = vm_info;
		/* Set up shared memory */
		if (id == 0) {
			struct cos_shm_rb *sm_rb;
			struct cos_shm_rb *sm_rb_r;

			printc("\tAllocating shared-memory (size: %lu)\n", (unsigned long) VM_SHM_ALL_SZ);
			vk_shmem_alloc(&vm_info, &vk_info, VK_VM_SHM_BASE, VM_SHM_ALL_SZ);

			printc("\tInitializing shared memory ringbuffers\n");
		        printc("\tFor recieving from Kernel...");
		        ret = vk_recv_rb_create(sm_rb_r, 1);
		        assert(ret);
		        printc("done\n");

		        printc("\tFor sending to Kernel...");
		        ret = vk_send_rb_create(sm_rb, 1);
		        assert(ret);
		        printc("done\n");
		} else {
			printc("\tMapping in shared-memory (size: %lu)\n", (unsigned long)VM_SHM_SZ);
			vk_shmem_map(&vm_info, &vk_info, VK_VM_SHM_BASE, VM_SHM_SZ);
		}

		if (id > 0) {
			sinvcap_t sinv;
			printc("\tSetting up sinv capability from kernel component to user component\n");
			sinv = cos_sinv_alloc(vk_cinfo, kernel_cinfo->comp_cap, (vaddr_t)__inv_test_fs);
			assert(sinv > 0);
			/* Copy into user capability table at a known location */
			ret = cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOSINV_BASE, vk_cinfo, sinv);
			assert(ret == 0);
			printc("Done setting up sinv\n");

			/* Create and copy booter comp virtual memory to each VM */
			vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
			assert(vm_range > 0);
			printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
			vk_virtmem_alloc(&vm_info, &vk_info, BOOT_MEM_VM_BASE, vm_range);

			/* Copy Kernel component after userspace component is initialized */
			if (id == 1) {
				vk_virtmem_alloc(&kernel_info, &vk_info, BOOT_MEM_VM_BASE, vm_range);
			}
		}

		printc("booter: VM%d Init END\n", id);
		vm_info.state = VM_RUNNING;
	}

	printc("------------------[ Booter & VMs init complete ]------------------\n");

	printc("\n------------------[ Starting Kernel ]--------------------\n");

	while (1) cos_thd_switch(kernel_info.initthd);

	printc("BACK IN BOOTER COMPONENT, DEAD END!\n");

	return;
}
