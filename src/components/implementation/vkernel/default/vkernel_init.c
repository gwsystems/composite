#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

thdcap_t vm_exit_thd = 0; /* exit thread created for each forked component, for graceful exit */
thdcap_t vk_termthd = 0; /* switch to this to shutdown */
thdcap_t vk_timer_thd = 0; /* timer thread - simple round-robin scheduler */
extern void vm_init(void *);
extern vaddr_t cos_upcall_entry;
struct cos_compinfo vkern_info;
sinvcap_t invcap;
thdcap_t vm_main_thd[COS_VIRT_MACH_COUNT];
thdid_t vm_main_thdid[COS_VIRT_MACH_COUNT];
int vm_blocked[COS_VIRT_MACH_COUNT];
arcvcap_t vminitrcv[COS_VIRT_MACH_COUNT];
arcvcap_t commrcv;
unsigned int ready_vms = COS_VIRT_MACH_COUNT;

void
vk_term_fn(void *d)
{
	BUG();
}

void
timer_fn(void) {
	static unsigned int i = 0;
	thdid_t tid;
	int rcving;
	cycles_t cycles;

	while (ready_vms) {
		int j;
		int pending;

		while ((pending = cos_rcv(commrcv, &tid, &rcving, &cycles)) > 0) { 
			if (tid) break;
		}
		//printc("%x %d\n", tid, rcving);
		for (j = 0; j < COS_VIRT_MACH_COUNT; j ++) {
			if(tid && vm_main_thdid[j] == tid) {
				if (!rcving) {
					printc("tid:%x unblocked\n", tid);
					vm_blocked[j] = 0;
				} else {
					printc("tid:%x blocked\n", tid);
					vm_blocked[j] = 1;
				}
				break;
			}
 		}

		int index = i ++ % COS_VIRT_MACH_COUNT;
		
		if (vm_main_thd[index] && !vm_blocked[index]) {
			cos_thd_switch(vm_main_thd[index]);
		}
	}
timer_done:
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

/* switch to vkernl booter thd */
void
vm_exit(void *id) 
{
	/* basically remove from READY list */
	ready_vms --;
	vm_main_thd[(int)id] = 0;
	vm_blocked[(int)id] = 0;
	/* do you want to spend time in printing? timer interrupt can screw with you, be careful */
	printc("VM %d Exiting\n", (int)id);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

extern void* vm_captbl_op_inv(long arg1, long arg2, long arg3, long arg4);
void
cos_init(void)
{
	printc("Hypervisor:vkernel START\n");
	struct cos_compinfo vmbooter_info[COS_VIRT_MACH_COUNT];

	int i = 0, id = 0;
	int page_range = 0;

	printc("Hypervisor:vkernel initializing\n");
	cos_meminfo_init(&vkern_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&vkern_info, -1, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, 
			(vaddr_t)BOOT_MEM_SHM_BASE, &vkern_info);


	vk_termthd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vk_term_fn, NULL);
	assert(vk_termthd);

	vk_timer_thd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, timer_fn, NULL);
	assert(vk_timer_thd);

	commrcv = cos_arcv_alloc(&vkern_info, vk_timer_thd, BOOT_CAPTBL_SELF_INITTCAP_BASE, vkern_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(commrcv);

	for (id = 0; id < COS_VIRT_MACH_COUNT; id ++) {
		printc("VM %d Initialization Start\n", id);
		thdcap_t vmthd;
		thdcap_t vmthd0;

		captblcap_t vmct;
		pgtblcap_t vmpt;
		compcap_t vmcc;

		tcap_t vmtcap, vmvirtcap;
		arcvcap_t vmvirtrcv;
		
		asndcap_t vmvirtsnd;

		printc("\tForking VM\n");
		vm_exit_thd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vm_exit, (void *)id);
		assert(vm_exit_thd);


		vmct = cos_captbl_alloc(&vkern_info);
		assert(vmct);

		vmpt = cos_pgtbl_alloc(&vkern_info);
		assert(vmpt);

		page_range = ((int)cos_get_heap_ptr() - BOOT_MEM_VM_BASE);

		vmcc = cos_comp_alloc(&vkern_info, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
		assert(vmcc);

		cos_meminfo_init(&vmbooter_info[id].mi, 
				BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
		cos_compinfo_init(&vmbooter_info[id], id, vmpt, vmct, vmcc,
				(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, 
				(vaddr_t)BOOT_MEM_SHM_BASE, &vkern_info);

		vm_main_thd[id] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vm_init, (void *)id);
		assert(vm_main_thd[id]);
		vm_main_thdid[id] = (thdid_t)cos_introspect(&vkern_info, vm_main_thd[id], 9);
		vm_blocked[id] = 0;
		printc("\tMain thread= cap:%x tid:%x\n", vm_main_thd[id], vm_main_thdid[id]);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITTHD_BASE, &vkern_info, vm_main_thd[id]);

		/*
		 * Set some fixed mem pool requirement. 64MB - for ex. 
		 * Allocate as many pte's 
		 * Map contiguous untyped memory for that size to those PTE's 
		 * Set cos_meminfo for vm accordingly.
		 * cos_untyped_alloc(ci, size)
		 */

		printc("\tCopying required capabilities\n");
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_CT, &vkern_info, vmct);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_PT, &vkern_info, vmpt);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_COMP, &vkern_info, vmcc);
		/* 
		 * TODO: We need seperate such capabilities for each VM. Can't use the BOOTER ones. 
		 */
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITHW_BASE, &vkern_info, BOOT_CAPTBL_SELF_INITHW_BASE); 
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_EXITTHD_BASE, &vkern_info, vm_exit_thd); 

		printc("\tCreating other required initial capabilities\n");
		vmtcap = cos_tcap_split(&vkern_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, 0);
		assert(vmtcap);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITTCAP_BASE, &vkern_info, vmtcap);

		vminitrcv[id] = cos_arcv_alloc(&vkern_info, vm_main_thd[id], vmtcap, vkern_info.comp_cap, commrcv);
		assert(vminitrcv[id]);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITRCV_BASE, &vkern_info, vminitrcv[id]);


		/*
		 * Create a new memory hole
		 * Copy as much memory as vkernel has typed.. 
		 * Map untyped memory to vkernel
		 */
		printc("\tMapping in Booter component Virtual memory\n");
		for (i = 0; i < page_range; i += PAGE_SIZE) {
			// allocate page
			vaddr_t spg = (vaddr_t) cos_page_bump_alloc(&vkern_info);
			// copy mem - can even do it after creating and copying all pages.
			memcpy((void *) spg, (void *) (BOOT_MEM_VM_BASE + i), PAGE_SIZE);
			// copy cap
			vaddr_t dpg = cos_mem_alias(&vmbooter_info[id], &vkern_info, spg);
		}

		if (!id) {
			printc("\tCreating shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_ALL_SZ);
			cos_shmem_alloc(&vmbooter_info[id], COS_SHM_ALL_SZ);
		} else {
			printc("\tMapping shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_VM_SZ);
			cos_shmem_map(&vmbooter_info[id], COS_SHM_VM_SZ);
		}

		printc("\tAllocating/Partitioning Untyped memory\n");
		cos_meminfo_alloc(&vmbooter_info[id], BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ);

		printc("VM %d Init DONE\n", id);
	}

	if (COS_VIRT_MACH_COUNT > 1) {
		printc("Setting up Cross VM (between vm0 and other vms) communication channels\n");

		int p, q;
		for (p = 1; p < COS_VIRT_MACH_COUNT; p ++) {

			asndcap_t ptozero, zerotop;
			//Create ASNYC END POINT BETWEEN p & q
			ptozero = cos_asnd_alloc(&vkern_info, vminitrcv[0], vkern_info.captbl_cap);
			assert(ptozero);
			cos_cap_cpy_at(&vmbooter_info[p], VM_CAPTBL_SELF_VTASND_SET_BASE, &vkern_info, ptozero);

			zerotop = cos_asnd_alloc(&vkern_info, vminitrcv[p], vkern_info.captbl_cap);
			assert(zerotop);
			cos_cap_cpy_at(&vmbooter_info[0], VM_CAPTBL_SELF_VTASND_SET_BASE + (p - 1) * CAP64B_IDSZ, &vkern_info, zerotop);
		}
	}

	printc("Starting Timer/Scheduler Thread\n");

	cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC, commrcv);
	printc("\t%d cycles per microsecond\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	printc("------------------[ Hypervisor & VMs init complete ]------------------\n");
	while (ready_vms) cos_thd_switch(vk_timer_thd);
	cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);
	printc("Timer thread DONE\n");

	printc("Hypervisor:vkernel END\n");
	cos_thd_switch(vk_termthd);
	printc("DEAD END\n");

	return;
}
