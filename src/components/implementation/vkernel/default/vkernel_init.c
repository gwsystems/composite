#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

extern void vm_init(void *);
//extern void term_fn(void *);
int is_vkernel = 1;
int test_status = 0;
extern vaddr_t cos_upcall_entry;
thdcap_t vmthd;
thdcap_t vmthd0;
struct cos_compinfo vkern_info;
struct cos_compinfo vmbooter_info;
//thdcap_t termthd; 		/* switch to this to shutdown */
captblcap_t vmct;
pgtblcap_t vmpt;
compcap_t vmcc;
sinvcap_t invcap;
extern void* vm_captbl_op_inv(long arg1, long arg2, long arg3, long arg4);

#define VM_MEM_KM_SIZE	(16*1024*1024)  //4MB
#define HEAP_MEM_SIZE	(16*1024*1024)

void
cos_init(void)
{
	printc("cos_init start\n");

	if (is_vkernel) { 

		int i = 0;
		int page_range = 0;

		cos_meminfo_init(&vkern_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
		cos_compinfo_init(&vkern_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &vkern_info);

		printc("\nvirtualization layer init\n");
		is_vkernel = 0;
		//termthd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, term_fn, NULL);
		//assert(termthd);
		printc("cos_upcall_entry: %x\n", (unsigned int)&cos_upcall_entry);

		vmct = cos_captbl_alloc(&vkern_info);
		assert(vmct);

		vmpt = cos_pgtbl_alloc(&vkern_info);
		assert(vmpt);

		printc("%s:%d\n", __FILE__, __LINE__);
		vmcc = cos_comp_alloc(&vkern_info, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
		//vmcc = cos_comp_alloc(&vkern_info, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, (vaddr_t)&cos_upcall_entry);
		assert(vmcc);

		page_range = ((int)cos_get_heap_ptr() - BOOT_MEM_VM_BASE);
		//cos_meminfo_init(&vmbooter_info.mi, BOOT_MEM_PM_BASE + COS_MEM_USER_PA_SZ - VM_MEM_PM_SIZE, VM_MEM_PM_SIZE,
		//		 BOOT_MEM_KM_BASE + COS_MEM_KERN_PA_SZ - VM_MEM_KM_SIZE, VM_MEM_KM_SIZE);
		cos_meminfo_init(&vmbooter_info.mi, 
				 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
		cos_compinfo_init(&vmbooter_info, vmpt, vmct, vmcc,
			  (vaddr_t)BOOT_MEM_VM_BASE, BOOT_CAPTBL_FREE, &vkern_info);
		printc("%s:%d\n", __FILE__, __LINE__);

		vmthd = cos_thd_alloc(&vkern_info, vmbooter_info.comp_cap, vm_init, &vmbooter_info);
		assert(vmthd);
		printc("%s:%d\n", __FILE__, __LINE__);
		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_INITTHD_BASE, &vkern_info, vmthd);
		printc("%s:%d\n", __FILE__, __LINE__);

		/*
		 * Set some fixed mem pool requirement. 64MB - for ex. 
		 * Allocate as many pte's 
		 * Map contiguous untyped memory for that size to those PTE's 
		 * Set cos_meminfo for vm accordingly.
		 * cos_untyped_alloc(ci, size)
		 */
		//printc("%s:%d\n", __FILE__, __LINE__);
		//printc("%s:%d\n", __FILE__, __LINE__);

		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_CT, &vkern_info, vmct);
		printc("%s:%d\n", __FILE__, __LINE__);
		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_PT, &vkern_info, vmpt);
		printc("%s:%d\n", __FILE__, __LINE__);
		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_COMP, &vkern_info, vmcc);
		printc("%s:%d\n", __FILE__, __LINE__);
		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, &vkern_info, BOOT_CAPTBL_SELF_INITTCAP_BASE);
		printc("%s:%d\n", __FILE__, __LINE__);
		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_INITRCV_BASE, &vkern_info, BOOT_CAPTBL_SELF_INITRCV_BASE);
		printc("%s:%d\n", __FILE__, __LINE__);
		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_INITHW_BASE, &vkern_info, BOOT_CAPTBL_SELF_INITHW_BASE); 
		printc("%s:%d\n", __FILE__, __LINE__);
//		cos_cap_cpy_at(&vmbooter_info, termthd, &vkern_info, termthd); 

		/*
		 * Create a new memory hole
		 * Copy as much memory as vkernel has typed.. 
		 * Map untyped memory to vkernel
		 */
		for (i = 0; i < page_range; i += PAGE_SIZE) {
		//printc("%s:%d\n", __FILE__, __LINE__);
			// allocate page
			vaddr_t spg = (vaddr_t) cos_page_bump_alloc(&vkern_info);
			// copy mem - can even do it after creating and copying all pages.
			memcpy((void *) spg, (void *) (BOOT_MEM_VM_BASE + i), PAGE_SIZE);
			//printc("dst pgtbl:%x src pgtbl:%x src pg:%x\n", vmbooter_info.pgtbl_cap, vkern_info.pgtbl_cap, spg);
			// copy cap
			vaddr_t dpg = cos_mem_alias(&vmbooter_info, &vkern_info, spg);
		//printc("%s:%d - %d\n", __FILE__, __LINE__, i);
			//printc("dst pgtbl:%x dst pg:%x src pgtbl:%x src pg:%x\n", vmbooter_info.pgtbl_cap, dpg, vkern_info.pgtbl_cap, spg);
			//cos_mem_alias(vmbooter_info.pgtbl_cap, , vkern_info.pgtbl_cap, pg);
		}
		printc("%s:%d\n", __FILE__, __LINE__);
		
		//cos_mem_partition(&vmbooter_info, BOOT_MEM_KM_BASE, VM_MEM_KM_SIZE);
		cos_meminfo_alloc(&vmbooter_info, BOOT_MEM_KM_BASE, VM_MEM_KM_SIZE);
//		invcap = cos_sinv_alloc(&vkern_info, vkern_info.comp_cap, (vaddr_t)vm_captbl_op_inv);
//		assert(invcap);
//		printc("%s:%d\n", __FILE__, __LINE__);
//		cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_CT, &vkern_info, invcap);
//		vmbooter_info.captbl_cap = invcap;
		printc("%s:%d\n", __FILE__, __LINE__);

		while (test_status == 0) cos_thd_switch(vmthd);

		printc("\n...done. terminating..\n");

		//cos_thd_switch(termthd);
	} 
	printc("cos_init end\n");

	return;
}
