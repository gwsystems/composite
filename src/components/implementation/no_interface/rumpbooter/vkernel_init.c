#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <vkern_api.h>
#include "cos_sync.h"
#include "vk_api.h"

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

thdcap_t vk_termthd; /* switch to this to shutdown */
extern void vm_init(void *);
extern vaddr_t cos_upcall_entry;
struct cos_compinfo vkern_info;
struct cos_compinfo vkern_shminfo;
unsigned int ready_vms = COS_VIRT_MACH_COUNT;

u64_t iters_per_usec = 0ULL;
unsigned int cycs_per_usec = 0;

/*worker thds for dlvm*/
//extern void dl_work_one(void *);

/*
 * Init caps for each VM
 */
cycles_t total_credits;
tcap_t vminittcap[COS_VIRT_MACH_COUNT];
int vm_cr_reset[COS_VIRT_MACH_COUNT];
int vm_bootup[COS_VIRT_MACH_COUNT];
thdcap_t vm_main_thd[COS_VIRT_MACH_COUNT];
thdcap_t vm_exit_thd[COS_VIRT_MACH_COUNT];
thdid_t vm_main_thdid[COS_VIRT_MACH_COUNT];
arcvcap_t vminitrcv[COS_VIRT_MACH_COUNT];
asndcap_t vksndvm[COS_VIRT_MACH_COUNT];
tcap_res_t vmcredits[COS_VIRT_MACH_COUNT];
tcap_prio_t vmprio[COS_VIRT_MACH_COUNT];
int vmstatus[COS_VIRT_MACH_COUNT];

/*
 * I/O transfer caps from VM0 <=> VMx
 */
thdcap_t vm0_io_thd[COS_VIRT_MACH_COUNT-1];
arcvcap_t vm0_io_rcv[COS_VIRT_MACH_COUNT-1];
asndcap_t vm0_io_asnd[COS_VIRT_MACH_COUNT-1];
/*
 * I/O transfer caps from VMx <=> VM0
 */
thdcap_t vms_io_thd[COS_VIRT_MACH_COUNT-1];
arcvcap_t vms_io_rcv[COS_VIRT_MACH_COUNT-1];
asndcap_t vms_io_asnd[COS_VIRT_MACH_COUNT-1];

thdcap_t sched_thd;
tcap_t sched_tcap;
arcvcap_t sched_rcv;

asndcap_t chtoshsnd;

void
vk_term_fn(void *d)
{
	BUG();
}

extern int vmid;

void
vm0_io_fn(void *d) 
{
	int line;
	unsigned int irqline;
	arcvcap_t rcvcap;
	printc("d: %d\n", (int)d);
	switch((int)d) {
		case DL_VM:
			line = 0;
			break;
		case 1:
			line = 13;
			irqline = IRQ_VM1;
			break;
		/*case 2:
			line = 15;
			irqline = IRQ_VM2;
			break;*/
		default: assert(0);
	}

	rcvcap = VM0_CAPTBL_SELF_IORCV_SET_BASE + (((int)d - 1) * CAP64B_IDSZ);
	printc("---------------------------rcvcap %d\n", (int)rcvcap);
	while (1) {
		int pending = cos_rcv(rcvcap);
	//	tcap_res_t budget = (tcap_res_t)cos_introspect(&vkern_info, vm0_io_tcap[DL_VM-1], TCAP_GET_BUDGET);
	//       if(budget < 60000) printc("budget: %lu\n", budget);
	//	printc("line %d\n", (int)line);
		if (line == 0) continue;
		intr_start(irqline);
		bmk_isr(line);
		cos_vio_tcap_set((int)d);
		intr_end();
	}
}

void
vmx_io_fn(void *d)
{
	printc("vmx\n");
	assert((int)d != DL_VM);
	while (1) {
		int pending = cos_rcv(VM_CAPTBL_SELF_IORCV_BASE);
		//continue;
		intr_start(IRQ_DOM0_VM);
		bmk_isr(12);
		intr_end();
	}
}

void
setup_credits(void)
{
	int i;
	
	total_credits = 0;

	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
		if (vmstatus[i] != VM_EXITED) {
			switch (i) {
				case 0:
					vmcredits[i] = TCAP_RES_INF;
					total_credits += (DOM0_CREDITS * VM_TIMESLICE * cycs_per_usec);
					break;
				case 1:
					vmcredits[i] = TCAP_RES_INF;
					total_credits += (VM1_CREDITS * VM_TIMESLICE * cycs_per_usec);
					//vmcredits[i] = (VM1_CREDITS * VM_TIMESLICE * cycs_per_usec);
					//total_credits += vmcredits[i];
					break;
				case 2:
					//vmcredits[i] = (VM2_CREDITS * VM_TIMESLICE * cycs_per_usec);
					vmcredits[i] = TCAP_RES_INF;
					total_credits += (VM2_CREDITS * VM_TIMESLICE * cycs_per_usec);
					//total_credits += vmcredits[i];
					break;
				default: assert(0);
					vmcredits[i] = VM_TIMESLICE;
					break;
			}
		} 
	}
}

void
reset_credits(void)
{
	struct vm_node *vm;
	int i;

	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
		vm_cr_reset[i] = 1;
		if (vmstatus[i] == VM_EXPENDED) 
			vmstatus[i] = VM_RUNNING;
	}
}

void
fillup_budgets(void)
{
	vmprio[0] = DOM0_PRIO;
	vm_cr_reset[0] = 1;
	
	vmprio[1] = NWVM_PRIO;
	vm_cr_reset[1] = 1;

	vmprio[2]   = DLVM_PRIO;
	vm_cr_reset[2] = 1;
}

volatile cycles_t start = 0, end = 0;


static inline u64_t spin_cycs(u64_t) __attribute__((optimize("O0")));
static inline u64_t spin_iters(u64_t) __attribute__((optimize("O0")));
static inline void spin_iters_per_usec(void) __attribute__((optimize("O0")));
#define CALIB_ITERS 1024
static inline u64_t
spin_iters(u64_t total_iters)
{
	volatile u64_t *s, *e, iters = 0;

	s = &start;
	e = &end;

	rdtscll(*s);
	do {
		iters ++;
	} while (iters < total_iters);
	rdtscll(*e);

	return *e - *s;
}

static inline u64_t 
spin_cycs(u64_t cycs)
{
	volatile u64_t *s, *e, iters = 0;

	s = &start;
	e = &end;
	rdtscll(*e);
	*e += cycs;

	do {
		iters ++;
		rdtscll(*s);
	} while (*s < *e);

	return iters;
}
/*
 * Calibrates iters_per_usec.
 * Caution: Call this at the bootup phase for the first time. 
 *	    Call to this any other time, can make it inaccurate with preemptions, etc!
 */
static inline void 
spin_iters_per_usec(void)
{
	int iter = 0;
	u64_t max_iters = (1ULL<<((sizeof(u64_t)*8)-1));
	u64_t avg_iters = 0ULL, total_iters = 0ULL;
	cycles_t calib = 1000;
	cycles_t cycs = cycs_per_usec * calib; /* cycs per usec */
	//u64_t this_iters[CALIB_ITERS] = { 0ULL };

	assert(cycs_per_usec);
	if (iters_per_usec) return;

	while (iter < CALIB_ITERS) {
		u64_t this_iters = 0ULL;

		this_iters = spin_cycs(cycs);

		assert (this_iters < max_iters);
		total_iters += this_iters;
		iter ++;
	}

	assert (total_iters < max_iters);
	avg_iters = total_iters / CALIB_ITERS;
	assert (avg_iters < max_iters);
	iters_per_usec = avg_iters / calib;
//	for (iter = 0 ; iter < CALIB_ITERS ; iter ++) {
//		printc("Iter%d: %llu\n", iter, this_iters[iter]);
//	}
	printc("%d Total:%llu, Average:%llu, LIMIT:%llu\n", CALIB_ITERS, total_iters, avg_iters, max_iters);
}

uint64_t t_vm_cycs  = 0;
uint64_t t_dom_cycs = 0;

#define YIELD_CYCS 10000
void
sched_fn(void *x)
{
	int sched_index = 0;
	thdid_t tid;
	int blocked;
	cycles_t cycles;
	printc("Scheduling VMs(Rumpkernel contexts)....\n");

	while (ready_vms) {
		int send = 1;
		tcap_res_t transfer_budget = TCAP_RES_INF, budget = 0;
		unsigned int count_over = 0;
		tcap_res_t dom0_max = DOM0_CREDITS * VM_TIMESLICE * cycs_per_usec;
		int index = 0;
		int ret;
		int pending = 0;

		index = sched_index;
		sched_index ++;
		sched_index %= COS_VIRT_MACH_COUNT;

		do {
			pending = cos_sched_rcv(sched_rcv, &tid, &blocked, &cycles);
			if (!tid || tid != vm_main_thdid[DL_VM]) continue;

			if (blocked) vmstatus[DL_VM] = VM_BLOCKED;
			else         vmstatus[DL_VM] = VM_RUNNING;
		} while (pending);

		if (vmstatus[index] != VM_RUNNING) continue;

		if (vm_cr_reset[index]) {
			send = 0;
			vm_cr_reset[index] = 0;
			transfer_budget = vmcredits[index];
		}

		if (send) {
			if (index == DL_VM) {
				do {
					ret = cos_switch(vm_main_thd[index], vminittcap[index], vmprio[index], 0, sched_rcv, cos_sched_sync());
					assert(ret == 0 || ret == -EAGAIN);
				} while (ret == -EAGAIN);
			} else {
				if (cos_asnd(vksndvm[index], 1)) assert(0);
			}
		} else {
			if (index == DL_VM) {
				if (cos_tcap_transfer(vminitrcv[index], sched_tcap, transfer_budget, vmprio[index])) assert(0);
				do {
					ret = cos_switch(vm_main_thd[index], vminittcap[index], vmprio[index], 0, sched_rcv, cos_sched_sync());
					assert(ret == 0 || ret == -EAGAIN);
				} while (ret == -EAGAIN);
			} else {
				if (cos_tcap_delegate(vksndvm[index], sched_tcap, transfer_budget, vmprio[index], TCAP_DELEG_YIELD)) assert(0);
			}
		}

	}
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
	//while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	while (1) cos_thd_switch(sched_thd);
}

void
vkold_shmem_alloc(struct cos_compinfo *vmci, unsigned int id, unsigned long shm_sz)
{
	unsigned long shm_ptr = BOOT_MEM_SHM_BASE;
	vaddr_t src_pg = (shm_sz * id) + shm_ptr, dst_pg, addr;

	assert(vmci);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	for (addr = shm_ptr ; addr < (shm_ptr + shm_sz) ; addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		/* VM0: mapping in all available shared memory. */
		src_pg = (vaddr_t)cos_page_bump_alloc(&vkern_shminfo);
		assert(src_pg && src_pg == addr);

		dst_pg = cos_mem_alias(vmci, &vkern_shminfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}	

	return;
}

void
vkold_shmem_map(struct cos_compinfo *vmci, unsigned int id, unsigned long shm_sz)
{
	unsigned long shm_ptr = BOOT_MEM_SHM_BASE;
	vaddr_t src_pg = (shm_sz * (id-1)) + shm_ptr, dst_pg, addr;

	assert(vmci);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	for (addr = shm_ptr ; addr < (shm_ptr + shm_sz) ; addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		/* VMx: mapping in only a section of shared-memory to share with VM0 */
		assert(src_pg);

		dst_pg = cos_mem_alias(vmci, &vkern_shminfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}	

	return;
}

void
cos_init(void)
{
	struct cos_compinfo vmbooter_info[COS_VIRT_MACH_COUNT];
	struct cos_compinfo vmbooter_shminfo[COS_VIRT_MACH_COUNT];
	assert(COS_VIRT_MACH_COUNT >= 2);

	printc("Hypervisor:vkernel START\n");

	int i = 0, id = 0, cycs;
	int page_range = 0;
	u64_t test_iters, test_total_iters, diff;

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per microsecond\n", cycs);

	cycs_per_usec = (unsigned int)cycs;
	//spin_iters_per_usec();

	//test_iters = iters_per_usec * 1000;
	//diff = spin_iters(test_iters);
	//printc("Spun for %llu iters = %llu cycs = %llu usecs\n", test_iters, diff, (diff)/cycs_per_usec);
	
	
	printc("cycles_per_usec: %lu, iters_per_usec:%llu TIME QUANTUM: %lu, RES_INF: %lu\n", (unsigned long)cycs_per_usec, iters_per_usec, (unsigned long)(VM_TIMESLICE*cycs_per_usec), TCAP_RES_INF);

	vm_list_init();
	setup_credits();
	fillup_budgets();

	printc("Hypervisor:vkernel initializing\n");
	cos_meminfo_init(&vkern_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&vkern_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &vkern_info);
	cos_compinfo_init(&vkern_shminfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)BOOT_MEM_SHM_BASE, BOOT_CAPTBL_FREE, &vkern_info);


	vk_termthd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vk_term_fn, NULL);
	assert(vk_termthd);

	printc("TCAPS INFRA to SIMULATE XEN ENV\n");

	printc("Initializing Timer/Scheduler Thread\n");
	sched_tcap = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	sched_thd = BOOT_CAPTBL_SELF_INITTHD_BASE;
	sched_rcv = BOOT_CAPTBL_SELF_INITRCV_BASE;

	chtoshsnd = cos_asnd_alloc(&vkern_info, sched_rcv, vkern_info.captbl_cap);
	assert(chtoshsnd);

	for (id = 0; id < COS_VIRT_MACH_COUNT; id ++) {
		printc("VM %d Initialization Start\n", id);
		captblcap_t vmct;
		pgtblcap_t vmpt, vmutpt;
		compcap_t vmcc;
		int ret;

		printc("\tForking VM\n");
		vm_exit_thd[id] = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vm_exit, (void *)id);
		assert(vm_exit_thd[id]);


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
		if (id == 0) { 
			cos_compinfo_init(&vmbooter_shminfo[id], vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_SHM_BASE, VM0_CAPTBL_FREE, &vkern_info);
			cos_compinfo_init(&vmbooter_info[id], vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, VM0_CAPTBL_FREE, &vkern_info);
		} else {
			cos_compinfo_init(&vmbooter_shminfo[id], vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_SHM_BASE, VM_CAPTBL_FREE, &vkern_info);
			cos_compinfo_init(&vmbooter_info[id], vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, &vkern_info);
		}

		vm_main_thd[id] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vm_init, (void *)id);
		assert(vm_main_thd[id]);
		vm_main_thdid[id] = (thdid_t)cos_introspect(&vkern_info, vm_main_thd[id], THD_GET_TID);
		printc("\tMain thread= cap:%x tid:%x\n", (unsigned int)vm_main_thd[id], vm_main_thdid[id]);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITTHD_BASE, &vkern_info, vm_main_thd[id]);
		vmstatus[id] = VM_RUNNING;
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_VKASND_BASE, &vkern_info, chtoshsnd);

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
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_EXITTHD_BASE, &vkern_info, vm_exit_thd[id]); 

		printc("\tCreating other required initial capabilities\n");
		vminittcap[id] = cos_tcap_alloc(&vkern_info);
		assert(vminittcap[id]);

		vminitrcv[id] = cos_arcv_alloc(&vkern_info, vm_main_thd[id], vminittcap[id], vkern_info.comp_cap, sched_rcv);
		assert(vminitrcv[id]);

		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITRCV_BASE, &vkern_info, vminitrcv[id]);
		cos_cap_cpy_at(&vmbooter_info[id], BOOT_CAPTBL_SELF_INITTCAP_BASE, &vkern_info, vminittcap[id]);

		/*
		 * Create send end-point to each VM's INITRCV end-point for scheduling.
		 */
		vksndvm[id] = cos_asnd_alloc(&vkern_info, vminitrcv[id], vkern_info.captbl_cap);
		assert(vksndvm[id]);

		if (id > 0) {
			printc("\tSetting up Cross VM (between vm0 and vm%d) communication channels\n", id);
			/* VM0 to VMid */
			vm0_io_thd[id-1] = cos_thd_alloc(&vkern_info, vmbooter_info[0].comp_cap, vm0_io_fn, (void *)id);
			assert(vm0_io_thd[id-1]);
			vms_io_thd[id-1] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vmx_io_fn, (void *)id);
			assert(vms_io_thd[id-1]);
			
			vm0_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vm0_io_thd[id-1], vminittcap[0], vkern_info.comp_cap, vminitrcv[0]);
			assert(vm0_io_rcv[id-1]);
			vms_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vms_io_thd[id-1], vminittcap[id], vkern_info.comp_cap, vminitrcv[id]);
			assert(vms_io_rcv[id-1]);

			/* Changing to init thd of dl_vm
			 * DOM0 -> DL_VM asnd
			 */
			if (id == DL_VM) {
				vm0_io_asnd[id-1] = cos_asnd_alloc(&vkern_info, vminitrcv[id], vkern_info.captbl_cap);
				assert(vm0_io_asnd[id-1]);
			} 
			else {
				vm0_io_asnd[id-1] = cos_asnd_alloc(&vkern_info, vms_io_rcv[id-1], vkern_info.captbl_cap);
				assert(vm0_io_asnd[id-1]);
			}

			vms_io_asnd[id-1] = cos_asnd_alloc(&vkern_info, vm0_io_rcv[id-1], vkern_info.captbl_cap);
			assert(vms_io_asnd[id-1]);

			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOTHD_SET_BASE + (id-1)*CAP16B_IDSZ, &vkern_info, vm0_io_thd[id-1]);
			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IORCV_SET_BASE + (id-1)*CAP64B_IDSZ, &vkern_info, vm0_io_rcv[id-1]);
			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOASND_SET_BASE + (id-1)*CAP64B_IDSZ, &vkern_info, vm0_io_asnd[id-1]);

			printc("VM_CAPTBL_SELF_IOASND_BASE%d\n", VM_CAPTBL_SELF_IOASND_BASE);
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
			
			vkold_shmem_alloc(&vmbooter_shminfo[id], id, COS_SHM_ALL_SZ + ((sizeof(struct cos_shm_rb *)*2)*(COS_VIRT_MACH_COUNT-1)) );
			for(i = 1; i < (COS_VIRT_MACH_COUNT); i++){
				printc("\tInitializing ringbufs for sending\n");
				struct cos_shm_rb *sm_rb = NULL;	
				vk_send_rb_create(sm_rb, i);
			}

			//allocating ring buffers for recving data
			for(i = 1; i < (COS_VIRT_MACH_COUNT); i++){
				printc("\tInitializing ringbufs for rcving\n");
				struct cos_shm_rb *sm_rb_r = NULL;	
				vk_recv_rb_create(sm_rb_r, i);
			}

		} else {
			printc("\tMapping shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_VM_SZ);
			vkold_shmem_map(&vmbooter_shminfo[id], id, COS_SHM_VM_SZ);
		}

		printc("\tAllocating/Partitioning Untyped memory\n");
		cos_meminfo_alloc(&vmbooter_info[id], BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ);

		printc("VM %d Init DONE\n", id);
	}

	//printc("sm_rb addr: %x\n", vk_shmem_addr_recv(2));
	printc("------------------[ Hypervisor & VMs init complete ]------------------\n");

	printc("Starting Timer/Scheduler Thread\n");
	sched_fn(NULL);
	printc("Timer thread DONE\n");

	printc("Hypervisor:vkernel END\n");
	cos_thd_switch(vk_termthd);
	printc("DEAD END\n");

	return;
}
