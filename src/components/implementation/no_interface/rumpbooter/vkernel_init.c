#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <vkern_api.h>
#include "cos_sync.h"
#include "vk_api.h"

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

extern void vm_init(void *);
extern vaddr_t cos_upcall_entry;
extern void *__inv_test_entry(int a, int b, int c);
//struct cos_compinfo vkern_info;

unsigned int cycs_per_usec;
uint64_t t_vm_cycs  = 0;
uint64_t t_dom_cycs = 0;

thdcap_t vm_main_thd;
thdid_t vm_main_thdid;

void
cos_init(void)
{
	struct cos_compinfo userspace;

	printc("Hypervisor:kernel component START\n");

	int i = 0, id = 0, cycs;
	int page_range = 0;

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init_OLD(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE,
			(vaddr_t)BOOT_MEM_SHM_BASE, &booter_info);

	captblcap_t vmct;
	pgtblcap_t vmpt, vmutpt;
	compcap_t vmcc;
	int ret;

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
        printc("\t%d cycles per microsecond\n", cycs);
	cycs_per_usec = (unsigned int)cycs;

	printc("\tForking VM\n");

	vmct = cos_captbl_alloc(&booter_info);
	assert(vmct);

	vmpt = cos_pgtbl_alloc(&booter_info);
	assert(vmpt);

	vmutpt = cos_pgtbl_alloc(&booter_info);
	assert(vmutpt);

	vmcc = cos_comp_alloc(&booter_info, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
	assert(vmcc);

	cos_meminfo_init(&userspace.mi,
			BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, vmutpt);

	cos_compinfo_init_OLD(&userspace, vmpt, vmct, vmcc,
			(vaddr_t)BOOT_MEM_VM_BASE, VM0_CAPTBL_FREE,
			(vaddr_t)BOOT_MEM_SHM_BASE, &booter_info);

	vm_main_thd = cos_thd_alloc(&booter_info, userspace.comp_cap, vm_init, (void *)1);
	assert(vm_main_thd);
	vm_main_thdid = (thdid_t)cos_introspect(&booter_info, vm_main_thd, THD_GET_TID);
	printc("\tMain thread= cap:%x tid:%x\n", (unsigned int)vm_main_thd, vm_main_thdid);
	cos_cap_cpy_at(&userspace, BOOT_CAPTBL_SELF_INITTHD_BASE, &booter_info, vm_main_thd);

	/*
	 * Set some fixed mem pool requirement. 64MB - for ex.
	 * Allocate as many pte's
	 * Map contiguous untyped memory for that size to those PTE's
	 * Set cos_meminfo for vm accordingly.
	 * cos_untyped_alloc(ci, size)
	 */

	printc("\tCopying required capabilities\n");
	cos_cap_cpy_at(&userspace, BOOT_CAPTBL_SELF_CT, &booter_info, vmct);
	cos_cap_cpy_at(&userspace, BOOT_CAPTBL_SELF_PT, &booter_info, vmpt);
	cos_cap_cpy_at(&userspace, BOOT_CAPTBL_SELF_UNTYPED_PT, &booter_info, vmutpt);
	cos_cap_cpy_at(&userspace, BOOT_CAPTBL_SELF_COMP, &booter_info, vmcc);
	/*
	 * TODO: We need seperate such capabilities for VM. Can't use the BOOTER ones.
	 */
	cos_cap_cpy_at(&userspace, BOOT_CAPTBL_SELF_INITHW_BASE, &booter_info, BOOT_CAPTBL_SELF_INITHW_BASE);

	printc("\tCreating other required initial capabilities\n");

	/* Create sinv capability from Userspace to Booter components */
	/* Need id 1 because we need to wait for id 0 to finish initializing */
	sinvcap_t sinv;

	sinv = cos_sinv_alloc(&booter_info, booter_info.comp_cap, (vaddr_t)__inv_test_entry);
	assert(sinv > 0);
	printc("sinv: %d\n", sinv);

	/* Copy into vm0 capability table at a known location */
	cos_cap_cpy_at(&userspace, VM0_CAPTBL_SELF_IOSINV_BASE, &booter_info, sinv);

	/*
	 * Create a new memory hole
	 * Copy as much memory as vkernel has typed..
	 * Map untyped memory to vkernel
	 */
	page_range = ((int)cos_get_heap_ptr() - BOOT_MEM_VM_BASE);
	/*
	 * Map the VM0's Virtual Memory only after I/O Comm Caps are allocated with all other VMs.
	 * Basically, when we're done with the last VM's INIT. (we could do it outside the Loop too.)
	 */
	printc("\tMapping in Booter component Virtual memory to VM%d, Range: %u\n", id, page_range);
	for (i = 0; i < page_range; i += PAGE_SIZE) {
		// allocate page
		vaddr_t spg = (vaddr_t) cos_page_bump_alloc(&booter_info);
		// copy mem - can even do it after creating and copying all pages.
		memcpy((void *) spg, (void *) (BOOT_MEM_VM_BASE + i), PAGE_SIZE);
		// copy cap
		vaddr_t dpg = cos_mem_alias(&userspace, &booter_info, spg);
	}

	printc("\tAllocating/Partitioning Untyped memory\n");
	cos_meminfo_alloc(&userspace, BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ);

	printc("VM %d Init DONE\n", id);

	printc("------------------[ Hypervisor & VMs init complete ]------------------\n");

	printc("------------------[ Booting RK ]--------------------\n");
	printc("rump_booter_init\n");
	/* RK will switch to user space thread when it is done booting */
	rump_booter_init();

	printc("DEAD END\n");

	return;
}
