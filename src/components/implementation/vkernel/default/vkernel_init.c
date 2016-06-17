#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

#define NO_OF_VMS	3

#define VM_MEM_KM_SIZE	(1<<25)  //16MB
#define HEAP_MEM_SIZE	(1<<23) //8MB

/* TODO: shared memory + async notification api. thinking, it should be part of cos_kernel_api.c and not here */
#define SHARED_MEM_SIZE	(1<<20) //1MB
#define SHARED_MEM_START_PA	0x20000000
#define SHARED_MEM_META_START_PA SHARED_MEM_START_PA
#define SHARED_MEM_DATA_START_PA (SHARED_MEM_META_START_PA + (1<<16)) //+ 64KB
#define SHARED_MEM_PA_SZ	(1<<23) //8MB

thdcap_t vm_exit_thd = 0;
thdcap_t vk_termthd = 0;
extern void vm_init(void *);
//extern void term_fn(void *);
int is_vkernel = 1;
int test_status = 1;
extern vaddr_t cos_upcall_entry;
struct cos_compinfo vkern_info;
//thdcap_t termthd; 		/* switch to this to shutdown */
sinvcap_t invcap;

void
vk_term_fn(void *d)
{
	BUG();
}

/* switch to vkernl booter thd */
void
vm_exit(void) 
{
	test_status = 0;
	while(1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}
static void *
cos_va2pa(void * vaddr)
{
        int paddr = call_cap_op(BOOT_CAPTBL_SELF_PT, CAPTBL_OP_INTROSPECT, (int)vaddr, 0,0,0);
        paddr = (paddr & 0xfffff000) | ((int)vaddr & 0x00000fff);
        return (void *)paddr;
}


extern void* vm_captbl_op_inv(long arg1, long arg2, long arg3, long arg4);
void
cos_init(void)
{
	printc("cos_init start\n");

	if (is_vkernel) { 

		int i = 0, id = 0;
		int page_range = 0;

		cos_meminfo_init(&vkern_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
		cos_compinfo_init(&vkern_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &vkern_info);

		printc("%x\n", cos_va2pa(BOOT_MEM_KM_BASE));
		//cos_cap_cpy_at (&vkern_info, BOOT_CAPTBL_SELF_INITTHD_BASE, &vkern_info, BOOT_CAPTBL_SELF_EXITTHD_BASE);
		printc("\nvirtualization layer init\n");
		is_vkernel = 0;

		vk_termthd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vk_term_fn, NULL);
		assert(vk_termthd);

		printc("cos_upcall_entry: %x\n", (unsigned int)&cos_upcall_entry);

		for (id = 0; id < NO_OF_VMS; id ++) {
			thdcap_t vmthd;
			thdcap_t vmthd0;

			captblcap_t vmct;
			pgtblcap_t vmpt;
			compcap_t vmcc;

		printc("%s:%d\n", __FILE__, __LINE__);
		vm_exit_thd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vm_exit, NULL);
		assert(vm_exit_thd);
		printc("%s:%d\n", __FILE__, __LINE__);

			struct cos_compinfo vmbooter_info;
			printc("VM %d Init Start\n", id);

			vmct = cos_captbl_alloc(&vkern_info);
			assert(vmct);

			vmpt = cos_pgtbl_alloc(&vkern_info);
			assert(vmpt);

			page_range = ((int)cos_get_heap_ptr() - BOOT_MEM_VM_BASE);

			printc("%s:%d\n", __FILE__, __LINE__);
			vmcc = cos_comp_alloc(&vkern_info, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
			//vmcc = cos_comp_alloc(&vkern_info, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, (vaddr_t)&cos_upcall_entry);
			assert(vmcc);

			//cos_meminfo_init(&vmbooter_info.mi, BOOT_MEM_PM_BASE + COS_MEM_USER_PA_SZ - VM_MEM_PM_SIZE, VM_MEM_PM_SIZE,
			//		 BOOT_MEM_KM_BASE + COS_MEM_KERN_PA_SZ - VM_MEM_KM_SIZE, VM_MEM_KM_SIZE);
			cos_meminfo_init(&vmbooter_info.mi, 
					BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
			cos_compinfo_init(&vmbooter_info, vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, BOOT_CAPTBL_FREE, &vkern_info);
			printc("%s:%d\n", __FILE__, __LINE__);

			vmthd = cos_thd_alloc(&vkern_info, vmbooter_info.comp_cap, vm_init, (void *)id);
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
			cos_cap_cpy_at(&vmbooter_info, BOOT_CAPTBL_SELF_EXITTHD_BASE, &vkern_info, vm_exit_thd); 
			printc("%s:%d\n", __FILE__, __LINE__);

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


			/* need shared memory for capturing this status */
			test_status = 1;
			while(test_status) cos_thd_switch(vmthd);

			printc("\nVM %d Init DONE\n", id);
		}
		//cos_thd_switch(termthd);
	} 
	printc("cos_init end\n");
	cos_thd_switch(vk_termthd);

	return;
}
