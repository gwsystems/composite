#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <vkern_api.h>
#include "cos_sync.h"

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

thdcap_t vm_exit_thd; /* exit thread created for each forked component, for graceful exit */
thdcap_t vk_termthd; /* switch to this to shutdown */
thdcap_t vk_sched_thd; /* sched thread - simple round-robin scheduler */
extern void vm_init(void *);
extern vaddr_t cos_upcall_entry;
struct cos_compinfo vkern_info;
unsigned int ready_vms = COS_VIRT_MACH_COUNT;

/*
 * Init caps for each VM
 */
tcap_t vminittcap[COS_VIRT_MACH_COUNT];
thdcap_t vm_main_thd[COS_VIRT_MACH_COUNT];
thdid_t vm_main_thdid[COS_VIRT_MACH_COUNT];
int vm_blocked[COS_VIRT_MACH_COUNT];
arcvcap_t vminitrcv[COS_VIRT_MACH_COUNT];
asndcap_t vksndvm[COS_VIRT_MACH_COUNT];
int vmcredits[COS_VIRT_MACH_COUNT];
tcap_prio_t vmprio[COS_VIRT_MACH_COUNT];
tcap_res_t vmbudget[COS_VIRT_MACH_COUNT];
int vmstatus[COS_VIRT_MACH_COUNT];

#ifdef __INTELLIGENT_TCAPS__
/*
 * TCap transfer caps from VKERN <=> VM
 */
thdcap_t vk_time_thd[COS_VIRT_MACH_COUNT];
thdid_t vk_time_thdid[COS_VIRT_MACH_COUNT];
int vk_time_blocked[COS_VIRT_MACH_COUNT];
tcap_t vk_time_tcap[COS_VIRT_MACH_COUNT];
arcvcap_t vk_time_rcv[COS_VIRT_MACH_COUNT];
asndcap_t vk_time_asnd[COS_VIRT_MACH_COUNT];
/*
 * TCap transfer caps from VM <=> VKERN
 */
thdcap_t vms_time_thd[COS_VIRT_MACH_COUNT];
tcap_t vms_time_tcap[COS_VIRT_MACH_COUNT];
arcvcap_t vms_time_rcv[COS_VIRT_MACH_COUNT];
asndcap_t vms_time_asnd[COS_VIRT_MACH_COUNT];
#endif

/*
 * I/O transfer caps from VM0 <=> VMx
 */
thdcap_t vm0_io_thd[COS_VIRT_MACH_COUNT-1];
#ifdef __INTELLIGENT_TCAPS__
tcap_t vm0_io_tcap[COS_VIRT_MACH_COUNT-1];
#endif
arcvcap_t vm0_io_rcv[COS_VIRT_MACH_COUNT-1];
asndcap_t vm0_io_asnd[COS_VIRT_MACH_COUNT-1];
/*
 * I/O transfer caps from VMx <=> VM0
 */
thdcap_t vms_io_thd[COS_VIRT_MACH_COUNT-1];
#ifdef __INTELLIGENT_TCAPS__
tcap_t vms_io_tcap[COS_VIRT_MACH_COUNT-1];
#endif
arcvcap_t vms_io_rcv[COS_VIRT_MACH_COUNT-1];
asndcap_t vms_io_asnd[COS_VIRT_MACH_COUNT-1];

void
vk_term_fn(void *d)
{
	BUG();
}

#ifdef __INTELLIGENT_TCAPS__
void
vk_time_fn(void *d) 
{
	while (1) {
		int pending = cos_rcv(vk_time_rcv[(int)d]);
		printc("vkernel: rcv'd from vm %d\n", (int)d);
	}
}

void
vm_time_fn(void *d)
{
	while (1) {
		int pending = cos_rcv(VM_CAPTBL_SELF_TIMERCV_BASE);
		printc("%d: rcv'd from vkernel\n", (int)d);
	}
}
#endif

extern int vmid;

void
vm0_io_fn(void *d) 
{
	int line;
	arcvcap_t rcvcap;

	switch((int)d) {
		case 1:
			line = 13;
			break;
		case 2:
			line = 15;
			break;
		default: assert(0);
	}
	rcvcap = VM0_CAPTBL_SELF_IORCV_SET_BASE + (((int)d - 1) * CAP64B_IDSZ);
	while (1) {
		int pending = cos_rcv(rcvcap);
		intr_start(line);
	//	printc("%s = %d\n", __func__, (int)d);
		bmk_isr(line);
		intr_end();
	}
}

void
vmx_io_fn(void *d)
{
	while (1) {
		int pending = cos_rcv(VM_CAPTBL_SELF_IORCV_BASE);
		intr_start(12);
	//	printc("%s = %d\n", __func__, (int)d);
		bmk_isr(12);
		intr_end();
	}
}

void
refill_credits(void)
{
	int i;

	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
		if (vmstatus[i] != VM_EXITED) {
			switch (i) {
				case 0:
					vmcredits[i] = DOM0_CREDITS;
					break;
				case 1:
					vmcredits[i] = VM1_CREDITS;
					break;
				case 2:
					vmcredits[i] = VM2_CREDITS;
					break;
				default:
					vmcredits[i] = 0;
					break;
			}
		} 
	}
}

void
sched_fn(void) 
{
	static unsigned int i = 0;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	int index;

	/* Just run microbooter for now.. before moving forward to Real VMs.. */
	index = COS_VIRT_MACH_COUNT - 1;
	while (vmstatus[index] != VM_EXITED)
		cos_tcap_delegate(vksndvm[index], BOOT_CAPTBL_SELF_INITTCAP_BASE, vmbudget[index], vmprio[index], TCAP_DELEG_YIELD);

	printc("Scheduling VMs(Rumpkernel contexts)....\n");

#ifdef __SIMPLE_XEN_LIKE_TCAPS__
	/* if dom0 should have inf budget.. */
	index = 0;
	if (TCAP_RES_IS_INF(vmbudget[index]) && cos_tcap_transfer(vminitrcv[index], BOOT_CAPTBL_SELF_INITTCAP_BASE, vmbudget[index], vmprio[index])) assert(0);
#endif

	while (ready_vms) {
		int j;
		int count_vms = 0;

		assert (vmstatus[0] != VM_EXITED); /* yeah, dom0 better be available always */
		
		index = i ++ % (COS_VIRT_MACH_COUNT - 1);

		while (index && (!vmcredits[index] || vmstatus[index] == VM_EXITED)) {
			index = i ++ % (COS_VIRT_MACH_COUNT - 1);
			count_vms ++;

#ifdef __INTELLIGENT_TCAPS__
			if (count_vms == ready_vms) { /* TODO: For all VMs including DOM0.. */
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
			if (count_vms == ready_vms - 2) { /* DOM0 not credit-based, VM(n-1) is microbooter and should have exited.. */
#endif
				//printc("%s:%d\n", __func__, __LINE__);
				refill_credits();
			}
		}
		
		//printc("%s: %d - %d\n", __func__, index, vmcredits[index]);
		assert (vmstatus[index] != VM_EXITED);
		/* need special handling for VM_BLOCKED. Because vkernel is a potential notifier for vms.. */
		//printc("Scheduling:%d ", index);
#ifdef __INTELLIGENT_TCAPS__
		{ /* delegate for all VMS, including dom0.. */
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
		if (index == 0) {
			cos_asnd(vksndvm[index]);
		} else {
#endif
			if(index) vmcredits[index] --;
			//cycles_t start, end, total;

			//rdtscll(start);
			cos_tcap_delegate(vksndvm[index], BOOT_CAPTBL_SELF_INITTCAP_BASE, vmbudget[index], vmprio[index], TCAP_DELEG_YIELD);
			//rdtscll(end);

			//total = end - start;
			//if (index && !cycles_same(total, vmbudget[index])) {
			//	if (total < VM_TIMESLICE) vmbudget[index] = VM_TIMESLICE - total;
			//	else vmbudget[index] = VM_TIMESLICE;
			//}
			//printc("consumed: %llu, delegated: %llu\n", end - start, vmbudget[index]);
		}
		/* TODO: */

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles)) /* handle blocked/unblocked events */;
	}

	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

/* switch to vkernl booter thd */
void
vm_exit(void *id) 
{
	if (ready_vms > 1) assert((int)id); /* DOM0 cannot exit while other VMs are still running.. */

	/* basically remove from READY list */
	ready_vms --;
	vmstatus[(int)id] = VM_EXITED;
	/* do you want to spend time in printing? timer interrupt can screw with you, be careful */
	printc("VM %d Exiting\n", (int)id);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

void
setup_budgets(void)
{
	int i;

#ifdef __INTELLIGENT_TCAPS__
	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
	vmbudget[0] = TCAP_RES_INF;
	vmprio[0] = PRIO_BOOST;
	
	vmbudget[1] = VM_TIMESLICE;
	vmprio[1] = PRIO_BOOST;

	for (i = 2 ; i < COS_VIRT_MACH_COUNT ; i ++) {
#endif
		vmbudget[i] = VM_TIMESLICE;
		vmprio[i]   = PRIO_UNDER;
	}
}

void
cos_init(void)
{
	assert(COS_VIRT_MACH_COUNT >= 2);

	printc("Hypervisor:vkernel START\n");
	struct cos_compinfo vmbooter_info[COS_VIRT_MACH_COUNT];

	int i = 0, id = 0, cycs;
	int page_range = 0;

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per microsecond\n", cycs);

	printc("Hypervisor:vkernel initializing\n");
	cos_meminfo_init(&vkern_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&vkern_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, 
			(vaddr_t)BOOT_MEM_SHM_BASE, &vkern_info);


	vk_termthd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vk_term_fn, NULL);
	assert(vk_termthd);

	setup_budgets();
	refill_credits();

	for (id = 0; id < COS_VIRT_MACH_COUNT; id ++) {
		printc("VM %d Initialization Start\n", id);
		captblcap_t vmct;
		pgtblcap_t vmpt, vmutpt;
		compcap_t vmcc;
		int ret;

		printc("\tForking VM\n");
		vm_exit_thd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vm_exit, (void *)id);
		assert(vm_exit_thd);


		vmct = cos_captbl_alloc(&vkern_info);
		assert(vmct);

		vmpt = cos_pgtbl_alloc(&vkern_info);
		assert(vmpt);

		vmutpt = cos_pgtbl_alloc(&vkern_info);
		assert(vmutpt);

		vmcc = cos_comp_alloc(&vkern_info, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
		assert(vmcc);

		cos_meminfo_init(&vmbooter_info[id].mi, 
				BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, vmutpt);
		if (id == 0) 
			cos_compinfo_init(&vmbooter_info[id], vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, VM0_CAPTBL_FREE, 
					(vaddr_t)BOOT_MEM_SHM_BASE, &vkern_info);
		else 
			cos_compinfo_init(&vmbooter_info[id], vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, 
					(vaddr_t)BOOT_MEM_SHM_BASE, &vkern_info);

		vm_main_thd[id] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vm_init, (void *)id);
		assert(vm_main_thd[id]);
		vm_main_thdid[id] = (thdid_t)cos_introspect(&vkern_info, vm_main_thd[id], THD_GET_TID);
		printc("\tMain thread= cap:%x tid:%x\n", (unsigned int)vm_main_thd[id], vm_main_thdid[id]);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITTHD_BASE, &vkern_info, vm_main_thd[id]);
		vmstatus[id] = VM_RUNNING;

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
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_UNTYPED_PT, &vkern_info, vmutpt);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_COMP, &vkern_info, vmcc);
		/* 
		 * TODO: We need seperate such capabilities for each VM. Can't use the BOOTER ones. 
		 */
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITHW_BASE, &vkern_info, BOOT_CAPTBL_SELF_INITHW_BASE); 
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_EXITTHD_BASE, &vkern_info, vm_exit_thd); 

		printc("\tCreating other required initial capabilities\n");
		vminittcap[id] = cos_tcap_alloc(&vkern_info, TCAP_PRIO_MAX);
		assert(vminittcap[id]);

		vminitrcv[id] = cos_arcv_alloc(&vkern_info, vm_main_thd[id], vminittcap[id], vkern_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(vminitrcv[id]);

#ifdef __INTELLIGENT_TCAPS__
		if ((ret = cos_tcap_transfer(vminitrcv[id], BOOT_CAPTBL_SELF_INITTCAP_BASE, vmbudget[id], vmprio[id]))) {
			printc("\tTcap transfer failed %d\n", ret);
			assert(0);
		}
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
		/* don't transfer any budget here.. */
#endif
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITRCV_BASE, &vkern_info, vminitrcv[id]);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITTCAP_BASE, &vkern_info, vminittcap[id]);

		/*
		 * Create send end-point to each VM's INITRCV end-point for scheduling.
		 */
		vksndvm[id] = cos_asnd_alloc(&vkern_info, vminitrcv[id], vkern_info.captbl_cap);
		assert(vksndvm[id]);

#ifdef __INTELLIGENT_TCAPS__
		printc("\tCreating TCap transfer capabilities (Between VKernel and VM%d)\n", id);
		/* VKERN to VM */
		vk_time_tcap[id] = cos_tcap_alloc(&vkern_info, TCAP_PRIO_MAX);
		assert(vk_time_tcap[id]);
		vk_time_thd[id] = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vk_time_fn, (void *)id);
		assert(vk_time_thd[id]);
		vk_time_thdid[id] = (thdid_t)cos_introspect(&vkern_info, vk_time_thd[id], 9);
		vk_time_blocked[id] = 0;
		vk_time_rcv[id] = cos_arcv_alloc(&vkern_info, vk_time_thd[id], vk_time_tcap[id], vkern_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(vk_time_rcv[id]);

		if ((ret = cos_tcap_transfer(vk_time_rcv[id], BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX))) {
			printc("\tTcap transfer failed %d\n", ret);
			assert(0);
		}

		/* VM to VKERN */		
		vms_time_tcap[id] = cos_tcap_alloc(&vkern_info, TCAP_PRIO_MAX);
		assert(vms_time_tcap[id]);
		vms_time_thd[id] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vm_time_fn, (void *)id);
		assert(vms_time_thd[id]);
		vms_time_rcv[id] = cos_arcv_alloc(&vkern_info, vms_time_thd[id], vms_time_tcap[id], vkern_info.comp_cap, vminitrcv[id]);
		assert(vms_time_rcv[id]);

		if ((ret = cos_tcap_transfer(vms_time_rcv[id], vminittcap[id], TCAP_RES_INF, TCAP_PRIO_MAX))) {
			printc("\tTcap transfer failed %d\n", ret);
			assert(0);
		}

		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_TIMETCAP_BASE, &vkern_info, vms_time_tcap[id]);
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_TIMETHD_BASE, &vkern_info, vms_time_thd[id]);
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_TIMERCV_BASE, &vkern_info, vms_time_rcv[id]);

		vk_time_asnd[id] = cos_asnd_alloc(&vkern_info, vms_time_rcv[id], vkern_info.captbl_cap);
		assert(vk_time_asnd[id]);
		vms_time_asnd[id] = cos_asnd_alloc(&vkern_info, vk_time_rcv[id], vkern_info.captbl_cap);
		assert(vms_time_asnd[id]);
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_TIMEASND_BASE, &vkern_info, vms_time_asnd[id]);
#endif

		if (id > 0) {
			printc("\tSetting up Cross VM (between vm0 and vm%d) communication channels\n", id);
			/* VM0 to VMid */
			vm0_io_thd[id-1] = cos_thd_alloc(&vkern_info, vmbooter_info[0].comp_cap, vm0_io_fn, (void *)id);
			assert(vm0_io_thd[id-1]);
			vms_io_thd[id-1] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vmx_io_fn, (void *)id);
			assert(vms_io_thd[id-1]);
#ifdef __INTELLIGENT_TCAPS__
			vm0_io_tcap[id-1] = cos_tcap_alloc(&vkern_info, TCAP_PRIO_MAX);
			assert(vm0_io_tcap[id-1]);
			vm0_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vm0_io_thd[id-1], vm0_io_tcap[id-1], vkern_info.comp_cap, vminitrcv[0]);
			assert(vm0_io_rcv[id-1]);

			if ((ret = cos_tcap_transfer(vm0_io_rcv[id-1], vminittcap[0], TCAP_RES_INF, TCAP_PRIO_MAX))) {
				printc("\tTcap transfer failed %d\n", ret);
				assert(0);
			}
			/* VMp to VM0 */		
			vms_io_tcap[id-1] = cos_tcap_alloc(&vkern_info, TCAP_PRIO_MAX);
			assert(vms_io_tcap[id-1]);
			vms_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vms_io_thd[id-1], vms_io_tcap[id-1], vkern_info.comp_cap, vminitrcv[id]);
			assert(vms_io_rcv[id-1]);

			if ((ret = cos_tcap_transfer(vms_io_rcv[id-1], vminittcap[id], TCAP_RES_INF, TCAP_PRIO_MAX))) {
				printc("\tTcap transfer failed %d\n", ret);
				assert(0);
			}

			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOTCAP_SET_BASE + (id-1)*CAP16B_IDSZ, &vkern_info, vm0_io_tcap[id-1]);
			cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_IOTCAP_BASE, &vkern_info, vms_io_tcap[id-1]);

#elif defined __SIMPLE_XEN_LIKE_TCAPS__
			vm0_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vm0_io_thd[id-1], vminittcap[0], vkern_info.comp_cap, vminitrcv[0]);
			assert(vm0_io_rcv[id-1]);
			vms_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vms_io_thd[id-1], vminittcap[id], vkern_info.comp_cap, vminitrcv[id]);
			assert(vms_io_rcv[id-1]);
#endif

			vm0_io_asnd[id-1] = cos_asnd_alloc(&vkern_info, vms_io_rcv[id-1], vkern_info.captbl_cap);
			assert(vm0_io_asnd[id-1]);
			vms_io_asnd[id-1] = cos_asnd_alloc(&vkern_info, vm0_io_rcv[id-1], vkern_info.captbl_cap);
			assert(vms_io_asnd[id-1]);

			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOTHD_SET_BASE + (id-1)*CAP16B_IDSZ, &vkern_info, vm0_io_thd[id-1]);
			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IORCV_SET_BASE + (id-1)*CAP64B_IDSZ, &vkern_info, vm0_io_rcv[id-1]);
			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOASND_SET_BASE + (id-1)*CAP64B_IDSZ, &vkern_info, vm0_io_asnd[id-1]);

			cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_IOTHD_BASE, &vkern_info, vms_io_thd[id-1]);
			cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_IORCV_BASE, &vkern_info, vms_io_rcv[id-1]);
			cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_IOASND_BASE, &vkern_info, vms_io_asnd[id-1]);
		}

		/*
		 * Create a new memory hole
		 * Copy as much memory as vkernel has typed.. 
		 * Map untyped memory to vkernel
		 */
		page_range = ((int)cos_get_heap_ptr() - BOOT_MEM_VM_BASE);
		if (id != 0) {
			/* 
			 * Map the VM0's Virtual Memory only after I/O Comm Caps are allocated with all other VMs.
			 * Basically, when we're done with the last VM's INIT. (we could do it outside the Loop too.)
			 */
			if (id == COS_VIRT_MACH_COUNT - 1) {
				printc("\tMapping in Booter component Virtual memory to VM0, Range: %u\n", page_range);
				for (i = 0; i < page_range; i += PAGE_SIZE) {
					// allocate page
					vaddr_t spg = (vaddr_t) cos_page_bump_alloc(&vkern_info);
					// copy mem - can even do it after creating and copying all pages.
					memcpy((void *) spg, (void *) (BOOT_MEM_VM_BASE + i), PAGE_SIZE);
					// copy cap
					vaddr_t dpg = cos_mem_alias(&vmbooter_info[0], &vkern_info, spg);
				}

			}
			printc("\tMapping in Booter component Virtual memory to VM%d, Range: %u\n", id, page_range);
			for (i = 0; i < page_range; i += PAGE_SIZE) {
				// allocate page
				vaddr_t spg = (vaddr_t) cos_page_bump_alloc(&vkern_info);
				// copy mem - can even do it after creating and copying all pages.
				memcpy((void *) spg, (void *) (BOOT_MEM_VM_BASE + i), PAGE_SIZE);
				// copy cap
				vaddr_t dpg = cos_mem_alias(&vmbooter_info[id], &vkern_info, spg);
			}
		}

		if (id == 0) {
			printc("\tCreating shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_ALL_SZ);
			
			cos_shmem_alloc(&vmbooter_info[id], COS_SHM_ALL_SZ + ((sizeof(struct cos_shm_rb *)*2)*(COS_VIRT_MACH_COUNT-1)) );
			for(i = 1; i < (COS_VIRT_MACH_COUNT-1); i++){
				printc("\tInitializing ringbufs for sending\n");
				struct cos_shm_rb * sm_rb;	
				vk_send_rb_create(sm_rb, i);
			}

			//allocating ring buffers for recving data
			for(i = 1; i < (COS_VIRT_MACH_COUNT-1); i++){
				printc("\tInitializing ringbufs for rcving\n");
				struct cos_shm_rb * sm_rb_r;	
				vk_recv_rb_create(sm_rb_r, i);
			}

		} else {
			printc("\tMapping shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_VM_SZ);
			cos_shmem_map(&vmbooter_info[id], COS_SHM_VM_SZ);
		}

		printc("\tAllocating/Partitioning Untyped memory\n");
		cos_meminfo_alloc(&vmbooter_info[id], BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ);

		printc("VM %d Init DONE\n", id);
	}

	//printc("sm_rb addr: %x\n", vk_shmem_addr_recv(2));
	printc("------------------[ Hypervisor & VMs init complete ]------------------\n");

	printc("Starting Timer/Scheduler Thread\n");
	sched_fn();
	printc("Timer thread DONE\n");

	printc("Hypervisor:vkernel END\n");
	cos_thd_switch(vk_termthd);
	printc("DEAD END\n");

	return;
}
