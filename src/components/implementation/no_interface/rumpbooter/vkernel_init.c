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

unsigned int cycs_per_usec;

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

#if defined(__INTELLIGENT_TCAPS__)
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
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
tcap_t vm0_io_tcap[COS_VIRT_MACH_COUNT-1];
#endif
arcvcap_t vm0_io_rcv[COS_VIRT_MACH_COUNT-1];
asndcap_t vm0_io_asnd[COS_VIRT_MACH_COUNT-1];
/*
 * I/O transfer caps from VMx <=> VM0
 */
thdcap_t vms_io_thd[COS_VIRT_MACH_COUNT-1];
#if defined(__INTELLIGENT_TCAPS__)
tcap_t vms_io_tcap[COS_VIRT_MACH_COUNT-1];
#endif
arcvcap_t vms_io_rcv[COS_VIRT_MACH_COUNT-1];
asndcap_t vms_io_asnd[COS_VIRT_MACH_COUNT-1];

thdcap_t sched_thd;
tcap_t sched_tcap;
arcvcap_t sched_rcv;

asndcap_t chtoshsnd;
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
thdcap_t dom0_sla_thd;
tcap_t dom0_sla_tcap;
arcvcap_t dom0_sla_rcv;
asndcap_t dom0_sla_snd, vktoslasnd;
#endif

void
vk_term_fn(void *d)
{
	BUG();
}

#if defined(__INTELLIGENT_TCAPS__)
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
	unsigned int irqline;
	arcvcap_t rcvcap;

	switch((int)d) {
		case 1:
			line = 13;
			irqline = IRQ_VM1;
			break;
		case 2:
			line = 15;
			irqline = IRQ_VM2;
			break;
		default: assert(0);
	}

	rcvcap = VM0_CAPTBL_SELF_IORCV_SET_BASE + (((int)d - 1) * CAP64B_IDSZ);
	while (1) {
		int pending = cos_rcv(rcvcap);
		intr_start(irqline);
		bmk_isr(line);
		cos_vio_tcap_set((int)d);
		intr_end();
	}
}

void
vmx_io_fn(void *d)
{
	while (1) {
		int pending = cos_rcv(VM_CAPTBL_SELF_IORCV_BASE);
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
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
					vmcredits[i] = (DOM0_CREDITS * VM_TIMESLICE * cycs_per_usec);
					total_credits += vmcredits[i];
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
					vmcredits[i] = TCAP_RES_INF;
					total_credits += (DOM0_CREDITS * VM_TIMESLICE * cycs_per_usec);
#endif
					break;
				case 1:
					vmcredits[i] = (VM1_CREDITS * VM_TIMESLICE * cycs_per_usec);
					total_credits += vmcredits[i];
					break;
				case 2:
					vmcredits[i] = (VM2_CREDITS * VM_TIMESLICE * cycs_per_usec);
					total_credits += vmcredits[i];
					break;
				default:
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

	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++)
		vm_cr_reset[i] = 1;

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	while ((vm = vm_next(&vms_expended)) != NULL) {
		vm_deletenode(&vms_expended, vm);
		vm_insertnode(&vms_runqueue, vm);
	}
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	while ((vm = vm_next(&vms_over)) != NULL) {
		vm_deletenode(&vms_over, vm);
		vm_insertnode(&vms_under, vm);
	}
#endif
}

void
fillup_budgets(void)
{
	int i = 0;

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++)
	{
		vmprio[i]   = PRIO_LOW;
		vm_cr_reset[i] = 1;
	}
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	vmprio[0] = PRIO_BOOST;
	vm_cr_reset[0] = 1;
	//vm_deletenode(&vms_under, &vmnode[0]);
	//vm_insertnode(&vms_boost, &vmnode[0]);
	
	vmprio[1] = PRIO_UNDER;
	vm_cr_reset[1] = 1;
	//vm_deletenode(&vms_under, &vmnode[1]);
	//vm_insertnode(&vms_boost, &vmnode[1]);

	for (i = 2 ; i < COS_VIRT_MACH_COUNT ; i ++)
	{
		vmprio[i]   = PRIO_UNDER;
		vm_cr_reset[i] = 1;
	}
#endif
}

/*
 * Avoiding credit based budgets at bootup.
 * Can be disabled by just setting this value to 0.
 *
 * A Vm1 with x budget > y of another vm2 slows down
 * bootup sequence because vm1 continuously blocks once
 * it's done with it's bootup..
 * Can see that visually.. This is just to fix that.
 * Credits at bootup shouldn't matter..!!
 */
int
bootup_sched_fn(int index, tcap_res_t budget)
{
	if (vm_bootup[index] < BOOTUP_ITERS) {
		tcap_res_t timeslice = VM_TIMESLICE * cycs_per_usec;

		vm_bootup[index] ++;
		if (budget >= timeslice) {
			if (cos_asnd(vksndvm[index], 1)) assert(0);
		} else {
			if (TCAP_RES_IS_INF(vmcredits[index])) {
				if (cos_tcap_delegate(vksndvm[index], sched_tcap, 0, vmprio[index], TCAP_DELEG_YIELD)) assert(0);
			} else {
				if (cos_tcap_delegate(vksndvm[index], sched_tcap, timeslice - budget, vmprio[index], TCAP_DELEG_YIELD)) assert(0);
			}
		}

		return 0;
	} else if (vm_bootup[index] == BOOTUP_ITERS) {
		vm_bootup[index] ++;
		printc("VM%d Bootup complete\n", index);
	}

	return 1;
}

uint64_t t_vm_cycs  = 0;
uint64_t t_dom_cycs = 0;

#define YIELD_CYCS 10000
void
sched_fn(void *x)
{
	thdid_t tid;
	int blocked;
	cycles_t cycles;
	int index, j;
	tcap_res_t cycs;
	cycles_t total_cycles = 0;
	int no_vms = 0;
	int done_printing = 0;
	cycles_t cpu_usage[COS_VIRT_MACH_COUNT];
	cycles_t cpu_cycs[COS_VIRT_MACH_COUNT];
	unsigned int usage[COS_VIRT_MACH_COUNT];
	unsigned int count[COS_VIRT_MACH_COUNT];
	cycles_t cycs_per_sec                   = cycs_per_usec * 1000 * 1000;
	cycles_t total_cycs                     = 0;
	unsigned int counter                    = 0;
	cycles_t start                          = 0;
	cycles_t end                            = 0;

	memset(cpu_usage, 0, sizeof(cpu_usage));
	memset(cpu_cycs, 0, sizeof(cpu_cycs));
	(void)cycs;

	printc("Scheduling VMs(Rumpkernel contexts)....\n");
	memset(vm_bootup, 0, sizeof(vm_bootup));

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	while (ready_vms) {
		struct vm_node *x, *y;
		unsigned int count_over = 0;

		while ((x = vm_next(&vms_runqueue)) != NULL) {
			int index         = x->id;
			tcap_res_t budget = 0;
			int send          = 1;
			tcap_res_t sched_budget = 0, transfer_budget = 0;
			tcap_res_t min_budget = VM_MIN_TIMESLICE * cycs_per_usec;

			if (unlikely(vmstatus[index] == VM_EXITED)) {
				vm_deletenode(&vms_runqueue, x); 
				vm_insertnode(&vms_exit, x);
				continue;
			}

			budget = (tcap_res_t)cos_introspect(&vkern_info, vminittcap[index], TCAP_GET_BUDGET);
			sched_budget = (tcap_res_t)cos_introspect(&vkern_info, sched_tcap, TCAP_GET_BUDGET);
			
			if (!bootup_sched_fn(index, budget)) continue;

			if (cycles_same(budget, 0, VK_CYCS_DIFF_THRESH) && !vm_cr_reset[index]) {
				vm_deletenode(&vms_runqueue, &vmnode[index]);
				vm_insertnode(&vms_expended, &vmnode[index]);
				count_over ++;

				if (count_over == ready_vms) {
					reset_credits();
					count_over = 0;
					no_vms = 0;
					if (done_printing) {
						memset(cpu_cycs, 0, sizeof(cpu_cycs));
						memset(cpu_usage, 0, sizeof(cpu_usage));
						done_printing = 0;
					}
				} else {
					continue;
				}
			} 

			if (vm_cr_reset[index]) {
				send = 0;

				transfer_budget = vmcredits[index] - budget;
				if (sched_budget <= (transfer_budget + min_budget)) {
					transfer_budget = 0;
				}

				if (sched_budget == 0) { 
					send = 1;
				} else {
					no_vms ++;
					vm_cr_reset[index] = 0;
				}
			}
			if (TCAP_RES_IS_INF(budget)) send = 1;

			rdtscll(start);
			if (send) {
				if (cos_asnd(vksndvm[index], 1)) assert(0);
			} else {
				//printc("%d b:%lu t:%lu\n", index, budget, transfer_budget);
				/*if (index == IO_BOUND_VM || index == 0) cpu_cycs[IO_BOUND_VM] += vmcredits[index];
				else*/ cpu_cycs[index] += vmcredits[index];
				if (cos_tcap_delegate(vksndvm[index], sched_tcap, transfer_budget, vmprio[index], TCAP_DELEG_YIELD)) assert(0);
			}
			rdtscll(end);

#if defined(PRINT_CPU_USAGE)
			tcap_res_t cpu_bound_usage = 0;
			tcap_res_t total_usage = (end - start);

			if (index == CPU_BOUND_VM) {
				tcap_res_t now;

				now = (tcap_res_t)cos_introspect(&vkern_info, vminittcap[index], TCAP_GET_BUDGET);
				if (now != 0) {
					printc("%lu\n", now); assert(0);
				}
				cpu_bound_usage = (transfer_budget + budget - now); /* get used amount of budget in cycles */
				cpu_usage[index] += cpu_bound_usage;

				cpu_usage[IO_BOUND_VM] += (total_usage - cpu_bound_usage); /* get time spent outside cpu bound vm */
			} else {
				if (index == 0 && total_usage < YIELD_CYCS)
					cpu_usage[index] += total_usage;
				else
					cpu_usage[IO_BOUND_VM] += total_usage;
			}
			total_cycs += total_usage;

			if (total_cycs >= cycs_per_sec) {
				int i;

				//assert(cpu_cycs[0] == 0 && cpu_usage[0] == 0);

				for (i = 1 ; i < COS_VIRT_MACH_COUNT ; i ++) {
					unsigned int vm_usage = (unsigned int)((cpu_usage[i] * 100) / cpu_cycs[i]);
					if (cpu_cycs[i]) printc("vm%d:%u%% ", i, vm_usage);
				}
				printc(" / %us\n", (unsigned int)((total_cycs) / cycs_per_sec));
				total_cycs = 0;
				
				done_printing = 1;
			}
#endif

			if (no_vms == COS_VIRT_MACH_COUNT) total_cycles += (end - start);
			if (total_cycles >= total_credits) {
				reset_credits();
				count_over = 0;
				no_vms = 0;
				total_cycles = 0;

				if (done_printing) {
					memset(cpu_cycs, 0, sizeof(cpu_cycs));
					memset(cpu_usage, 0, sizeof(cpu_usage));
					done_printing = 0;
				}
			}

			while (cos_sched_rcv(sched_rcv, &tid, &blocked, &cycles)) {
				/* if a VM is blocked.. just move it to the end of the queue..*/
				if (tid && blocked) {
					int i;
	
					for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
						if (vm_main_thdid[i] == tid && x->prev != &vmnode[i]) {
							/* push it to the end of the queue.. */
							vm_deletenode(&vms_runqueue, &vmnode[i]);
							vm_insertnode(&vms_runqueue, &vmnode[i]);	
						}
					}
				} 
			}

		}
	}
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	while (ready_vms) {
		struct vm_node *x, *y;
		unsigned int count_over = 0;
		tcap_res_t dom0_max = DOM0_CREDITS * VM_TIMESLICE * cycs_per_usec;

#if 0
		while ((x = vm_next(&vms_boost)) != NULL) { /* if there is at least one element in boost prio.. */
			int index = x->id;

			if (unlikely(vmstatus[index] == VM_EXITED)) {
				vm_deletenode(&vms_boost, x); 
				vm_insertnode(&vms_exit, x);
				continue;
			}
			/* TODO: if boosted prio, do I care about the budgets, credits etc? */
			//printc("%s:%d - %d\n", __func__, __LINE__, index);
			cos_tcap_delegate(vksndvm[index], BOOT_CAPTBL_SELF_INITTCAP_BASE, vmcredits[index], vmprio[index], TCAP_DELEG_YIELD);
			while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &blocked, &cycles)) ;
		}
#endif

		while ((x = vm_next(&vms_under)) != NULL) {
			int index         = x->id;
			tcap_res_t budget = 0, transfer_budget = 0;
			int send          = 1;

			if (unlikely(vmstatus[index] == VM_EXITED)) {
				vm_deletenode(&vms_under, x); 
				vm_insertnode(&vms_exit, x);
				continue;
			}

			budget = (tcap_res_t)cos_introspect(&vkern_info, vminittcap[index], TCAP_GET_BUDGET);

			if (!bootup_sched_fn(index, budget)) continue;

			if (index && cycles_same(budget, 0, VK_CYCS_DIFF_THRESH) && !vm_cr_reset[index]) {
				vm_deletenode(&vms_under, &vmnode[index]);
				vm_insertnode(&vms_over, &vmnode[index]);
				count_over ++;

				if (count_over == ready_vms - 1) {
					reset_credits();
					count_over = 0;
					no_vms = 0;
				} else {
					continue;
				}
			} 

			if (vm_cr_reset[index]) {
				send = 0;
				no_vms ++;
				vm_cr_reset[index] = 0;

				transfer_budget = vmcredits[index] - budget;
			}
			if (TCAP_RES_IS_INF(budget)) send = 1;

			rdtscll(start);
			if (send) {
				if (cos_asnd(vksndvm[index], 1)) assert(0);
			} else {
				if (cos_tcap_delegate(vksndvm[index], sched_tcap, transfer_budget, vmprio[index], TCAP_DELEG_YIELD)) assert(0);
			}
			rdtscll(end);

			if (no_vms == COS_VIRT_MACH_COUNT - 1) total_cycles += (end - start);
			if (total_cycles >= total_credits) {
				reset_credits();
				count_over = 0;
				no_vms = 0;
				total_cycles = 0;
			}

#if defined(PRINT_CPU_USAGE)
			unsigned int usg;

			if (index) {
				usg = budget;
				if (!send) usg += transfer_budget;
			} else {
				usg = dom0_max;
			}
			usg = ((end - start) * 100) / usg;

			usage[index] += usg;
			count[index] ++;

			if (index) cpu_cycs[index] = send ? budget : (budget + transfer_budget);
			else       cpu_cycs[index] = dom0_max;
			cpu_usage[index] = (end - start);
			total_cycs      += (end - start);

			if (total_cycs >= cycs_per_sec) {
				for (j = 0 ; j < COS_VIRT_MACH_COUNT ; j ++) {
					//if (cpu_cycs[j])
					//	printc("vm%d:%u%% ", j, (unsigned int)((cpu_usage[j] * 100) / cpu_cycs[j]));
						//printc("vm%d:%llu:%llu ", j, cpu_usage[j], cpu_cycs[j]);

					if (count[index]) {
						printc("%d:%u%% ", j, (usage[j] / count[j]));
					}

					cpu_usage[j] = cpu_cycs[j] = 0;
					usage[j] = 0;
					count[j] = 0;
				}
				printc("/ %usecs-%u\n", (unsigned int)(total_cycs/cycs_per_sec), counter);
				total_cycs = 0;
				counter ++;
			}
#endif

			while (cos_sched_rcv(sched_rcv, &tid, &blocked, &cycles)) ;
		}
	}
#endif
}

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
cycles_t dom0_sla_act_cyc = 0;

thdcap_t dummy_thd = 0;
arcvcap_t dummy_rcv = 0;
void
dummy_fn(void *x)
{ while (1) cos_rcv(dummy_rcv); } 

void
dom0_sla_fn(void *x)
{
	//static cycles_t prev = 0;
	int vmid = 0;
	struct cos_compinfo btr_info;

	cos_meminfo_init(&btr_info.mi, BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);

	cos_compinfo_init(&btr_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), VM0_CAPTBL_FREE, &btr_info);

	while (1) {
		tcap_res_t sla_budget;
		thdid_t tid;
		int blocked;
		cycles_t cycles;//, now = 0, act = 0;
		asndcap_t vm_snd;
		//int i = 0;
		
		//cos_sched_rcv(VM0_CAPTBL_SELF_SLARCV_BASE, &tid, &blocked, &cycles);
		sla_budget = (tcap_res_t)cos_introspect(&btr_info, VM0_CAPTBL_SELF_SLATCAP_BASE, TCAP_GET_BUDGET);
		//rdtscll(now);
		//act = (now - prev);
		//prev = now;
		
		rdtscll(dom0_sla_act_cyc);
		//printc("DOM0 SLA Activated: %llu : %lu : %llu\n", act, sla_budget, dom0_sla_act_cyc);

		//if(sla_budget) if (cos_tcap_delegate(VM0_CAPTBL_SELF_SLASND_BASE, VM0_CAPTBL_SELF_SLATCAP_BASE, 0, vmprio[0], 0)) assert(0);
		if(sla_budget) if (cos_tcap_transfer(BOOT_CAPTBL_SELF_INITRCV_BASE, VM0_CAPTBL_SELF_SLATCAP_BASE, 0, vmprio[0])) assert(0);
	}
}

void
chronos_fn(void *x)
{
	//static cycles_t prev = 0;

	while (1) {
		tcap_res_t total_budget = TCAP_RES_INF;
		tcap_res_t sla_slice = VM_TIMESLICE * cycs_per_usec;
		thdid_t tid;
		int blocked;
		cycles_t cycles;//, now = 0, act = 0;
		struct vm_node *x, *y;

		//cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &blocked, &cycles);
		//tcap_res_t sched_budget = (tcap_res_t)cos_introspect(&vkern_info, sched_tcap, TCAP_GET_BUDGET);
		//tcap_res_t sla_budget = (tcap_res_t)cos_introspect(&vkern_info, dom0_sla_tcap, TCAP_GET_BUDGET);
		//rdtscll(now);
		//act = (now - prev);
		//prev = now;
		
/*		y = vm_next(&vms_runqueue);
		assert(y != NULL);

		x = y;
		do {
			int index = x->id;

			total_budget += vmcredits[index];
			x = vm_next(&vms_runqueue);

		} while (x != y);
		vm_prev(&vms_runqueue);
*/
		//total_budget *= SCHED_QUANTUM;
		//printc("Chronos activated :%llu: %lu:%lu-%lu:%lu-%d\n", act, sched_budget, total_budget, sla_budget, sla_slice, SCHED_QUANTUM);

		//if (cos_tcap_delegate(vktoslasnd, BOOT_CAPTBL_SELF_INITTCAP_BASE, sla_slice, PRIO_LOW, TCAP_DELEG_YIELD)) assert(0);
		if (cos_tcap_transfer(dom0_sla_rcv, BOOT_CAPTBL_SELF_INITTCAP_BASE, sla_slice, PRIO_LOW)) assert(0);
		//printc("%s:%d\n", __func__, __LINE__);
		/* additional cycles so vk doesn't allocate less budget to one of the vms.. */
		if (cos_tcap_delegate(chtoshsnd, BOOT_CAPTBL_SELF_INITTCAP_BASE, total_budget, PRIO_LOW, TCAP_DELEG_YIELD)) assert(0);
		//rdtscll(act);
		//printc("%llu:%llu:%llu\n", now, act, act - now);

	}	
}
#endif

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
	vaddr_t src_pg = (shm_sz * id) + shm_ptr, dst_pg, addr;

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

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per microsecond\n", cycs);

	cycs_per_usec = (unsigned int)cycs;

	printc("cycles_per_usec: %lu, TIME QUANTUM: %lu, RES_INF: %lu\n", (unsigned long)cycs_per_usec, (unsigned long)(VM_TIMESLICE*cycs_per_usec), TCAP_RES_INF);

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

#if defined(__INTELLIGENT_TCAPS__)
	printc("TODO: Intelligent TCAPS INFRA.. OOPS!\n");
	assert(0);
#elif defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	printc("DISTRIBUTED TCAPS INFRA\n");
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	printc("TCAPS INFRA to SIMULATE XEN ENV\n");
#else
	assert(0);
#endif

	printc("Initializing Timer/Scheduler Thread\n");
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	sched_tcap = cos_tcap_alloc(&vkern_info);
	assert(sched_tcap);
	sched_thd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, sched_fn, (void *)0);
	assert(sched_thd);
	sched_rcv = cos_arcv_alloc(&vkern_info, sched_thd, sched_tcap, vkern_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(sched_rcv);
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	sched_tcap = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	sched_thd = BOOT_CAPTBL_SELF_INITTHD_BASE;
	sched_rcv = BOOT_CAPTBL_SELF_INITRCV_BASE;
#endif

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
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
		if (id == 0) {
			printc("\tCreating Time Management System Capabilities in DOM0\n");
			dom0_sla_tcap = cos_tcap_alloc(&vkern_info);
			assert(dom0_sla_tcap);
			dom0_sla_thd = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, dom0_sla_fn, (void *)id);
			assert(dom0_sla_thd);
			dom0_sla_rcv = cos_arcv_alloc(&vkern_info, dom0_sla_thd, dom0_sla_tcap, vkern_info.comp_cap, vminitrcv[id]);
			assert(dom0_sla_rcv);

			if (cos_cap_cpy_at(&vmbooter_info[id], VM0_CAPTBL_SELF_SLATCAP_BASE, &vkern_info, dom0_sla_tcap)) assert(0);
			if (cos_cap_cpy_at(&vmbooter_info[id], VM0_CAPTBL_SELF_SLATHD_BASE, &vkern_info, dom0_sla_thd)) assert(0);
			if (cos_cap_cpy_at(&vmbooter_info[id], VM0_CAPTBL_SELF_SLARCV_BASE, &vkern_info, dom0_sla_rcv)) assert(0);

			dom0_sla_snd = vksndvm[id];
			vktoslasnd = cos_asnd_alloc(&vkern_info, dom0_sla_rcv, vkern_info.captbl_cap);
			assert(vktoslasnd);
			if (cos_cap_cpy_at(&vmbooter_info[id], VM0_CAPTBL_SELF_SLASND_BASE, &vkern_info, dom0_sla_snd)) assert(0);

			dummy_thd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, dummy_fn, (void *)id);
			assert(dummy_thd);
			dummy_rcv = cos_arcv_alloc(&vkern_info, dummy_thd, dom0_sla_tcap, vkern_info.comp_cap, dom0_sla_rcv);
			assert(dummy_rcv);
		}
#endif

#if defined(__INTELLIGENT_TCAPS__)
		printc("\tCreating TCap transfer capabilities (Between VKernel and VM%d)\n", id);
		/* VKERN to VM */
		vk_time_tcap[id] = cos_tcap_alloc(&vkern_info);
		assert(vk_time_tcap[id]);
		vk_time_thd[id] = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, vk_time_fn, (void *)id);
		assert(vk_time_thd[id]);
		vk_time_thdid[id] = (thdid_t)cos_introspect(&vkern_info, vk_time_thd[id], 9);
		vk_time_blocked[id] = 0;
		vk_time_rcv[id] = cos_arcv_alloc(&vkern_info, vk_time_thd[id], vk_time_tcap[id], vkern_info.comp_cap, sched_rcv);
		assert(vk_time_rcv[id]);

		if ((ret = cos_tcap_transfer(vk_time_rcv[id], BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX))) {
			printc("\tTcap transfer failed %d\n", ret);
			assert(0);
		}

		/* VM to VKERN */		
		vms_time_tcap[id] = cos_tcap_alloc(&vkern_info);
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
#elif defined(__SIMPLE_DISTRIBUTED_TCAPS__) || defined(__SIMPLE_XEN_LIKE_TCAPS__)
		cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_VKASND_BASE, &vkern_info, chtoshsnd);
#endif

		if (id > 0) {
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
			/* DOM0 to have capability to delegate time to VM */
			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_INITASND_SET_BASE + (id-1)*CAP64B_IDSZ, &vkern_info, vksndvm[id]);
#endif
			printc("\tSetting up Cross VM (between vm0 and vm%d) communication channels\n", id);
			/* VM0 to VMid */
			vm0_io_thd[id-1] = cos_thd_alloc(&vkern_info, vmbooter_info[0].comp_cap, vm0_io_fn, (void *)id);
			assert(vm0_io_thd[id-1]);
			vms_io_thd[id-1] = cos_thd_alloc(&vkern_info, vmbooter_info[id].comp_cap, vmx_io_fn, (void *)id);
			assert(vms_io_thd[id-1]);
#if defined(__INTELLIGENT_TCAPS__)
			vm0_io_tcap[id-1] = cos_tcap_alloc(&vkern_info);
			assert(vm0_io_tcap[id-1]);
			vm0_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vm0_io_thd[id-1], vm0_io_tcap[id-1], vkern_info.comp_cap, vminitrcv[0]);
			assert(vm0_io_rcv[id-1]);

			if ((ret = cos_tcap_transfer(vm0_io_rcv[id-1], vminittcap[0], TCAP_RES_INF, TCAP_PRIO_MAX))) {
				printc("\tTcap transfer failed %d\n", ret);
				assert(0);
			}
			/* VMp to VM0 */		
			vms_io_tcap[id-1] = cos_tcap_alloc(&vkern_info);
			assert(vms_io_tcap[id-1]);
			vms_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vms_io_thd[id-1], vms_io_tcap[id-1], vkern_info.comp_cap, vminitrcv[id]);
			assert(vms_io_rcv[id-1]);

			if ((ret = cos_tcap_transfer(vms_io_rcv[id-1], vminittcap[id], TCAP_RES_INF, TCAP_PRIO_MAX))) {
				printc("\tTcap transfer failed %d\n", ret);
				assert(0);
			}

			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOTCAP_SET_BASE + (id-1)*CAP16B_IDSZ, &vkern_info, vm0_io_tcap[id-1]);
			cos_cap_cpy_at(&vmbooter_info[id], VM_CAPTBL_SELF_IOTCAP_BASE, &vkern_info, vms_io_tcap[id-1]);
#elif defined(__SIMPLE_DISTRIBUTED_TCAPS__)
			vm0_io_tcap[id-1] = cos_tcap_alloc(&vkern_info);
			assert(vm0_io_tcap[id-1]);
			vm0_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vm0_io_thd[id-1], vm0_io_tcap[id-1], vkern_info.comp_cap, vminitrcv[0]);
			assert(vm0_io_rcv[id-1]);

			vms_io_rcv[id-1] = cos_arcv_alloc(&vkern_info, vms_io_thd[id-1], vminittcap[id], vkern_info.comp_cap, vminitrcv[id]);
			assert(vms_io_rcv[id-1]);
			cos_cap_cpy_at(&vmbooter_info[0], VM0_CAPTBL_SELF_IOTCAP_SET_BASE + (id-1)*CAP16B_IDSZ, &vkern_info, vm0_io_tcap[id-1]);
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
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

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	/* should I switch to scheduler ?? */
//	cos_switch(sched_thd, BOOT_CAPTBL_SELF_INITTCAP_BASE, PRIO_LOW, TCAP_TIME_NIL, 0, cos_sched_sync());

	chronos_fn(NULL);
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	printc("Starting Timer/Scheduler Thread\n");
	sched_fn(NULL);
	printc("Timer thread DONE\n");
#endif

	printc("Hypervisor:vkernel END\n");
	cos_thd_switch(vk_termthd);
	printc("DEAD END\n");

	return;
}
