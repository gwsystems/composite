#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <vkern_api.h>
#include <vk_api.h>
#include <sinv_calls.h>
#include <shdmem.h>
#include "cos_sync.h"
#include "vk_types.h"
#include "vk_api.h"
#include "spin.h"

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

extern thdcap_t cos_cur;
extern void vm_init(void *);
extern void kernel_init(void *);
extern vaddr_t cos_upcall_entry;

struct vkernel_info vk_info;
struct cos_compinfo *vk_cinfo = (struct cos_compinfo *)&vk_info.cinfo;

struct vms_info user_info1;
struct cos_compinfo *user_cinfo1 = (struct cos_compinfo *)&user_info1.cinfo;
struct vms_info user_info2;
struct cos_compinfo *user_cinfo2 = (struct cos_compinfo *)&user_info2.cinfo;

struct vms_info kernel_info;
struct cos_compinfo *kernel_cinfo = (struct cos_compinfo *)&kernel_info.cinfo;

unsigned int ready_vms = VM_COUNT;

unsigned int cycs_per_usec = 0;
unsigned int cycs_per_msec = 0;

/*
 * Init caps for each VM
 */
cycles_t total_credits;
tcap_t vminittcap[VM_COUNT];
int vm_cr_reset[VM_COUNT];
int vm_bootup[VM_COUNT];
thdcap_t vm_main_thd[VM_COUNT];
thdcap_t vm_exit_thd[VM_COUNT];
thdid_t vm_main_thdid[VM_COUNT];
arcvcap_t vminitrcv[VM_COUNT];
asndcap_t vksndvm[VM_COUNT];
tcap_res_t vmcredits[VM_COUNT];
tcap_prio_t vmprio[VM_COUNT];
int vmstatus[VM_COUNT];
int runqueue[VM_COUNT];
cycles_t vmperiod[VM_COUNT];
cycles_t vmlastperiod[VM_COUNT];
cycles_t vmwakeup[VM_COUNT];
cycles_t vmlastwakeup[VM_COUNT];

u64_t vmruncount[VM_COUNT];

/*
 * I/O transfer caps from VM0 <=> VMx
 */
thdcap_t vm0_io_thd[VM_COUNT-1];
arcvcap_t vm0_io_rcv[VM_COUNT-1];
asndcap_t vm0_io_asnd[VM_COUNT-1];
/*
 * I/O transfer caps from VMx <=> VM0
 */
thdcap_t vms_io_thd[VM_COUNT-1];
arcvcap_t vms_io_rcv[VM_COUNT-1];
asndcap_t vms_io_asnd[VM_COUNT-1];

thdcap_t sched_thd;
tcap_t sched_tcap;
arcvcap_t sched_rcv;

asndcap_t chtoshsnd;

void
vk_term_fn(void *d)
{
	BUG();
}


void
vm0_io_fn(void *d)
{
	int line;
	unsigned int irqline;
	arcvcap_t rcvcap;
//	printc("d: %d\n", (int)d);
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
//	printc("---------------------------rcvcap %d\n", (int)rcvcap);
	while (1) {
		int pending = cos_rcv(rcvcap);
	//	tcap_res_t budget = (tcap_res_t)cos_introspect(vk_cinfo, vm0_io_tcap[DL_VM-1], TCAP_GET_BUDGET);
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
//	printc("vmx\n");
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

	//total_credits = 0;

	for (i = 0 ; i < VM_COUNT ; i ++) {
		vmwakeup[i] = 0;
		vmlastwakeup[i] = 0;
		vmcredits[i] = 0;
		vmperiod[i] = 0;
		vmlastperiod[i] = 0;
		if (vmstatus[i] != VM_EXITED) {
			switch (i) {
				case 0:
					vmcredits[i] = ((DOM0_CREDITS) * VM_TIMESLICE * cycs_per_usec);
					vmperiod[i] = (DOM0_PERIOD * VM_MS_TIMESLICE * cycs_per_msec);
					vmwakeup[i] = ((DOM0_WKUP_PERIOD) * VM_MS_TIMESLICE * cycs_per_msec);
					//vmcredits[i] = TCAP_RES_INF;
					//total_credits += (DOM0_CREDITS * VM_TIMESLICE * cycs_per_usec);
					break;
				case 1:
					if (CPU_VM < VM_COUNT) assert(CPU_VM == 1);
					vmcredits[i] = (VM1_CREDITS * VM_MS_TIMESLICE * cycs_per_msec);
					vmperiod[i] = (VM1_PERIOD * VM_MS_TIMESLICE * cycs_per_msec);
					vmwakeup[i] = (VM1_WKUP_PERIOD * VM_MS_TIMESLICE * cycs_per_msec);
					//vmcredits[i] = TCAP_RES_INF;
					//total_credits += (VM1_CREDITS * VM_TIMESLICE * cycs_per_usec);
					break;
				case 2: assert(i == DL_VM);
					vmcredits[i] = (VM2_CREDITS * VM_TIMESLICE * cycs_per_usec);
					vmcredits[i] += (DLVM_ADD_WORK * VM_TIMESLICE * cycs_per_usec);
					vmperiod[i] = (VM2_PERIOD * VM_MS_TIMESLICE * cycs_per_msec);
					//vmcredits[i] = TCAP_RES_INF;
					//total_credits += (VM2_CREDITS * VM_TIMESLICE * cycs_per_usec);
					break;
				default: assert(0);
					vmcredits[i] = VM_TIMESLICE;
					break;
			}
		}
	}
	total_credits = (10 * cycs_per_msec);
}

void
fillup_budgets(void)
{
	int i;

	vmprio[0] = DOM0_PRIO;
	vmprio[1] = NWVM_PRIO;
	vmprio[2] = DLVM_PRIO;

	assert(VM_COUNT == 3);
	assert(vmprio[0] != vmprio[1] && vmprio[1] != vmprio[2] && vmprio[2] != vmprio[0]);
	assert(vmprio[0] == PRIO_HIGH || vmprio[0] == PRIO_MID || vmprio[0] == PRIO_LOW);
	assert(vmprio[1] == PRIO_HIGH || vmprio[1] == PRIO_MID || vmprio[1] == PRIO_LOW);
	assert(vmprio[2] == PRIO_HIGH || vmprio[2] == PRIO_MID || vmprio[2] == PRIO_LOW);

//	runqueue[0] = 0;
//	runqueue[1] = DL_VM;
//	runqueue[2] = 1;

	for (i = 0 ; i < VM_COUNT ; i++) {
		vm_cr_reset[0] = 1;
		vmruncount[0] = 0;

		/*
		 * this relies on the fact that there are only 3vms.. and they all have
		 * different priorities..
		 */
		if (vmprio[i] == PRIO_HIGH) runqueue[0] = i;
		else if (vmprio[i] == PRIO_MID) runqueue[1] = i;
		else if (vmprio[i] == PRIO_LOW) runqueue[2] = i;
	}
}

/* call this for non-budget accounting case. */
static tcap_res_t
check_vm_budget(int index)
{
	tcap_res_t budget = (tcap_res_t)cos_introspect(vk_cinfo, vminittcap[index], TCAP_GET_BUDGET);
	tcap_res_t transfer_budget = vmcredits[index] - budget;

	if (TCAP_RES_IS_INF(budget) || budget >= vmcredits[index]) return budget;
	if (cos_tcap_transfer(vminitrcv[index], sched_tcap, transfer_budget, vmprio[index])) assert(0);

	return transfer_budget;
}

#undef VARIABLE_PERIODS
/* for budget accounting case */
static void
check_replenish_budgets(void)
{
#ifdef VARIABLE_PERIODS
	cycles_t now;
	int i;

	rdtscll(now);

	for (i = 0 ; i < VM_COUNT ; i ++) {
		if (!vmcredits[i] || (!vmperiod[i] && vmlastperiod[i])) continue; /* perhaps has inf! and was replenished once */

		if (vmlastperiod[i] == 0 || (now - vmlastperiod[i] >= vmperiod[i])) {
			tcap_res_t budget = (tcap_res_t)cos_introspect(vk_cinfo, vminittcap[i], TCAP_GET_BUDGET);
			tcap_res_t transfer_budget = vmcredits[i] - budget;

			vmlastperiod[i] = now;
			/* cpu vm is only unblocked on replenishment */
			if (i == CPU_VM && vmstatus[i] != VM_EXITED) vmstatus[i] = VM_RUNNING;
			if (TCAP_RES_IS_INF(budget) || budget >= vmcredits[i]) continue;
			if (cos_tcap_transfer(vminitrcv[i], sched_tcap, transfer_budget, vmprio[i])) assert(0);
		}
	}

#else
	static cycles_t last_replenishment = 0;
	cycles_t now;
	int i;

	rdtscll(now);
	if (last_replenishment == 0 || (now - last_replenishment >= total_credits)) {
		rdtscll(last_replenishment);

		for (i = 0 ; i < VM_COUNT ; i++) {
			tcap_res_t budget = (tcap_res_t)cos_introspect(vk_cinfo, vminittcap[i], TCAP_GET_BUDGET);
			tcap_res_t transfer_budget = vmcredits[i] - budget;

		//	if (i != DL_VM && vmstatus[i] == VM_BLOCKED) {
		//		vmstatus[i] = VM_RUNNING;
		//	}
			if (i != DL_VM && vmstatus[i] == VM_EXPENDED) vmstatus[i] = VM_RUNNING;

			if (TCAP_RES_IS_INF(budget) || budget >= vmcredits[i]) continue;


			if (cos_tcap_transfer(vminitrcv[i], sched_tcap, transfer_budget, vmprio[i])) assert(0);
		}
	}
#endif
}

#define WAKEUP_FIXED_PERIOD 1
#define VARIABLE_WAKEUP

/* wakeup API: wakes up blocked vms every x timeslices. */
static void
wakeup_vms(void)
{
#ifdef VARIABLE_WAKEUP
	cycles_t now;
	int i;

	rdtscll(now);
	for (i = 0 ; i < VM_COUNT ; i ++) {
		if (i == DL_VM || i == CPU_VM || vmstatus[i] == VM_EXITED) continue;

		if (vmlastwakeup[i] == 0 || (now - vmlastwakeup[i] >= vmwakeup[i])) {
			vmlastwakeup[i] = now;
			vmstatus[i] = VM_RUNNING;
		}
	}
#else
	static cycles_t last_wakeup = 0;
	cycles_t wakeupslice = VM_MS_TIMESLICE * WAKEUP_FIXED_PERIOD * cycs_per_msec;
	cycles_t now;

	rdtscll(now);
	if (last_wakeup == 0 || now - last_wakeup > wakeupslice) {
		int i;

		last_wakeup = now;
		for (i = 0 ; i < VM_COUNT ; i ++) {
			if (i == DL_VM || i == CPU_VM || vmstatus[i] == VM_EXITED) continue;
			if (vmstatus[i] == VM_EXPENDED) continue;

			vmstatus[i] = VM_RUNNING;
		}
	}
#endif
}

#define DOM0_BOOTUP 60
#undef JUST_RR

static int
sched_vm(void)
{
#ifdef JUST_RR
	static int sched_index = DL_VM;
	int index = sched_index;

	sched_index ++;
	sched_index %= VM_COUNT;

	return index;
#else
	static last = 0;
	int i;

	for (i = 0 ; i < VM_COUNT ; i++) {
//		if (runqueue[i] == 0 && i == 0) {
//			if (!last && vmruncount[i] == DOM0_BOOTUP) { last = 1; printc("Scheduling DOM0 for the last time..!\n"); }
//			if (vmruncount[i] > DOM0_BOOTUP) continue;
//		}
		if (vmstatus[runqueue[i]] == VM_RUNNING) return runqueue[i];
	}

	return -1;
#endif
}

uint64_t t_vm_cycs  = 0;
uint64_t t_dom_cycs = 0;

#define YIELD_CYCS 10000
void
sched_fn(void *x)
{
	u64_t vkernelruncount = 0;
	int sched_index = DL_VM;
	thdid_t tid;
	int blocked;
	cycles_t cycles;
	u64_t count_over[VM_COUNT] = { 0 };

	printc("Scheduling VMs(Rumpkernel contexts)....\n");

	assert(VM_RUNNING == 0);
	assert(VM_BLOCKED == 1);
	check_replenish_budgets();
	while (ready_vms) {
		int send = 1;
		tcap_res_t transfer_budget = TCAP_RES_INF, budget = 0;
		int index = 0;
		int ret;
		int pending = 0;

		//index = sched_index;
		//sched_index ++;
		//sched_index %= VM_COUNT;
		/* wakeup eligible vms.. */
		wakeup_vms();
		do {
			int i;
			int rcvd = 0;

			pending = cos_sched_rcv_all(sched_rcv, &rcvd, &tid, &blocked, &cycles);
			if (!tid) continue;

			if (CPU_VM < VM_COUNT && tid == vm_main_thdid[CPU_VM] && cycles) {
				vmstatus[CPU_VM] = VM_EXPENDED;
				continue;
			}
			for (i = 0 ; i < VM_COUNT ; i++) {
				if (tid == vm_main_thdid[i]) {
					if (!cycles) vmstatus[i] = blocked;
					else         vmstatus[i] = VM_EXPENDED;
					break;
				}
			}

		} while (pending);

		check_replenish_budgets();

		/* do not run cpu-bound vm for until dom0 is booted up */
		if (vmruncount[0] < DOM0_BOOTUP && CPU_VM < VM_COUNT) vmstatus[CPU_VM] = VM_BLOCKED;

		index = sched_vm();
		if (index < 0) continue;

#ifdef JUST_RR
		if (vmstatus[index] != VM_RUNNING) continue;
#else
		assert(vmstatus[index] == VM_RUNNING);
#endif
		//budget = check_vm_budget(index);
		//assert(budget);
		//if (vmstatus[DL_VM] == VM_RUNNING) index = DL_VM;

		vmruncount[index] ++;
		if (index == DL_VM || index == CPU_VM) {
			if (vmruncount[index] % 100 == 0) printc("%d:%llu\n", index, vmruncount[index]);
			do {
				ret = cos_switch(vm_main_thd[index], vminittcap[index], vmprio[index], 0, sched_rcv, cos_sched_sync());
				assert(ret == 0 || ret == -EAGAIN);
			} while (ret == -EAGAIN);
		} else {
			cycles_t now, then;
			rdtscll(then);
			if (cos_asnd(vksndvm[index], 1)) assert(0);
			rdtscll(now);

			if ((now - then) >= (VM_TIMESLICE * cycs_per_usec)) {
				count_over[index] ++;
			}
	//		if (vmruncount[index] % 10000 == 0) printc("%d:%llu, over:%llu\n", index, vmruncount[index], count_over[index]);
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

#define TEST_SPIN_N 5
static void
test_spinlib(void)
{
	int i;
	cycles_t start, end;
	u64_t usecs[TEST_SPIN_N] = { 1000, 10000, 5000, 4000, 8500 };
	u64_t cycs_usecs[TEST_SPIN_N] = { 1000, 10000, 5000, 4000, 8500 };

	for (i = 0 ; i < TEST_SPIN_N ; i++) {
		rdtscll(start);
		spin_usecs(usecs[i]);
		rdtscll(end);

		printc("%d = Spun (%llu us): %llu cycs, %llu usecs\n", i, usecs[i], end-start, (end-start)/cycs_per_usec);
	}

	for (i = 0 ; i < TEST_SPIN_N ; i++) {
		rdtscll(start);
		spin_cycles((cycs_usecs[i] * cycs_per_usec));
		rdtscll(end);

		printc("%d = Spun (%llu cycs): %llu cycs, %llu usecs\n", i, cycs_usecs[i], end-start, (end-start)/cycs_per_usec);
	}

}

void
cos_init(void)
{
	printc("Hypervisor:booter component START\n");

	int i = 0, id = 0, cycs;
	int page_range = 0;


	assert(VM_COUNT >= 2);

	printc("Hypervisor:vkernel START\n");

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per microsecond\n", cycs);
	cycs_per_usec = (unsigned int)cycs;

	while (!(cycs = cos_hw_cycles_per_msec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per millisecond\n", cycs);
	cycs_per_msec = (unsigned int)cycs;

	printc("Timeslice in ms(%lu):%lu\n", (unsigned long)cycs_per_msec, (unsigned long)(VM_MS_TIMESLICE * cycs_per_msec));
	printc("Timeslice in us(%lu):%lu\n", (unsigned long)cycs_per_usec, (unsigned long)(VM_TIMESLICE * cycs_per_usec));

	spin_calib();
	test_spinlib();

	setup_credits();
	fillup_budgets();

	/* Initialize kernel component */
	cos_meminfo_init(&vk_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(vk_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, vk_cinfo);

	printc("Hypervisor:vkernel initializing\n");

	printc("TCAPS INFRA to SIMULATE XEN ENV\n");

	printc("Initializing Timer/Scheduler Thread\n");
	sched_tcap = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	sched_thd = BOOT_CAPTBL_SELF_INITTHD_BASE;
	sched_rcv = BOOT_CAPTBL_SELF_INITRCV_BASE;

	chtoshsnd = cos_asnd_alloc(vk_cinfo, sched_rcv, vk_cinfo->captbl_cap);
	assert(chtoshsnd);

	/*
	 * NOTE:
	 * Fork kernel and user components
	 * kernel component has id = 0
	 * user component has id = 1
	 */
	printc("user_cinfo1: %p, user_cinfo2: %p, kernel_cinfo: %p\n", user_cinfo1, user_cinfo2, kernel_cinfo);

	/* Save temp struct cos_compinfo for copying kernel component virtual memory below */
	for (id = 0 ; id < VM_COUNT ; id++) {
		struct vms_info *vm_info;
		struct cos_compinfo *vm_cinfo;
		/* TODO, generalize this for any number of components */
                if (id == 0) {
			vm_info = &kernel_info;
			vm_cinfo = kernel_cinfo;
		} else if (id == 1) {
			vm_info = &user_info1;
			vm_cinfo = user_cinfo1;
		} else if (id == 2) {
			vm_info = &user_info2;
			vm_cinfo = user_cinfo2;
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

		cos_meminfo_init(&vm_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, vmutpt);
		if (id == 0) {
			cos_compinfo_init(vm_cinfo, vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, VM0_CAPTBL_FREE, vk_cinfo);
		} else {
			cos_compinfo_init(vm_cinfo, vmpt, vmct, vmcc,
					(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, vk_cinfo);
		}

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
			rk_initcaps_init(vm_info, &vk_info);
		} else if (id > 0)  {
			vk_initcaps_init(vm_info, &vk_info);
			printc("\tCoppying in vm_main_thd capability into Kernel component\n");
			/* TODO make this more generic, but only the user_vm1 gets its init thread in kernel_cinfo */
			if (id == 1) {
				ret = cos_cap_cpy_at(kernel_cinfo, BOOT_CAPTBL_USERSPACE_THD,
					     vm_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE);
			}
			assert(ret == 0);
		}
		vm_main_thd[id]   = vm_info->initthd;
		vm_main_thdid[id] = vm_info->inittid;

		vminittcap[id] = vm_info->inittcap;
		assert(vminittcap[id]);
		vminitrcv[id] =  vm_info->initrcv;
		assert(vminitrcv[id]);
		vksndvm[id] = vk_info.vminitasnd[id];
		assert(vksndvm[id]);

		printc("\tDone Initializing capabilities\n");

		/* User component must be initializing and kernel component must be done, wait for id > 0 */
		/* FIXME, right now userspace vm is the only one who gets sinv capbilities */
		if (id == 1) {
			assert(DL_VM == VM_COUNT-1);

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
		}

		printc("\tSetting up Cross VM (between vm0 and vm%d) communication channels\n", id);
		if (id > 0) {
			if (id != DL_VM) {
				/* VM0 to VMid */
				vm0_io_thd[id-1] = cos_thd_alloc(vk_cinfo, vm_cinfo->comp_cap, vm0_io_fn, (void *)id);
				assert(vm0_io_thd[id-1]);
				vms_io_thd[id-1] = cos_thd_alloc(vk_cinfo, vm_cinfo->comp_cap, vmx_io_fn, (void *)id);
				assert(vms_io_thd[id-1]);

				printc("vk_cinfo: %p, vm0_io_thd[id-1]: %p, vminitcap[0]: %d, vk_cinfo->comp_cap: %d, vminitrcv[0]: %p\n", vk_cinfo, vm0_io_thd[id-1], vminittcap[0], vk_cinfo->comp_cap, vminitrcv[0]);
				vm0_io_rcv[id-1] = cos_arcv_alloc(vk_cinfo, vm0_io_thd[id-1], vminittcap[0], vk_cinfo->comp_cap, vminitrcv[0]);
				assert(vm0_io_rcv[id-1]);
				vms_io_rcv[id-1] = cos_arcv_alloc(vk_cinfo, vms_io_thd[id-1], vminittcap[id], vk_cinfo->comp_cap, vminitrcv[id]);
				assert(vms_io_rcv[id-1]);

				/* Changing to init thd of dl_vm
				 * DOM0 -> DL_VM asnd
				 */
				/* FIXME, how could this ever happen? */
				if (id == DL_VM) {
					vm0_io_asnd[id-1] = cos_asnd_alloc(vk_cinfo, vminitrcv[id], vk_cinfo->captbl_cap);
					assert(vm0_io_asnd[id-1]);
				} else {
					vm0_io_asnd[id-1] = cos_asnd_alloc(vk_cinfo, vms_io_rcv[id-1], vk_cinfo->captbl_cap);
					assert(vm0_io_asnd[id-1]);
				}

				vms_io_asnd[id-1] = cos_asnd_alloc(vk_cinfo, vm0_io_rcv[id-1], vk_cinfo->captbl_cap);
				assert(vms_io_asnd[id-1]);

				cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOTHD_SET_BASE + (id-1)*CAP16B_IDSZ, vk_cinfo, vm0_io_thd[id-1]);
				cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IORCV_SET_BASE + (id-1)*CAP64B_IDSZ, vk_cinfo, vm0_io_rcv[id-1]);
				cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOASND_SET_BASE + (id-1)*CAP64B_IDSZ, vk_cinfo, vm0_io_asnd[id-1]);

				/* TODO, figure out the propper boot setup and if we need these */
				//cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_SELF_IOTHD_BASE, vk_cinfo, vms_io_thd[id-1]);
				//cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_SELF_IORCV_BASE, vk_cinfo, vms_io_rcv[id-1]);
				//cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_SELF_IOASND_BASE, vk_cinfo, vms_io_asnd[id-1]);
			} else {
				vm0_io_asnd[id-1] = cos_asnd_alloc(vk_cinfo, vminitrcv[id], vk_cinfo->captbl_cap);
				assert(vm0_io_asnd[id-1]);
				cos_cap_cpy_at(vm_cinfo, VM0_CAPTBL_SELF_IOASND_SET_BASE + (id-1)*CAP64B_IDSZ, vk_cinfo, vm0_io_asnd[id-1]);
			}

			/* Create and copy booter comp virtual memory to each VM */
			vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
			assert(vm_range > 0);
			printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
			vk_virtmem_alloc(vm_info, vk_cinfo, BOOT_MEM_VM_BASE, vm_range);



			/* Copy Kernel component after userspace component is initialized */
			if (id == 1) {
				vk_virtmem_alloc(&kernel_info, vk_cinfo, BOOT_MEM_VM_BASE, vm_range);
			}
		}

		/* TODO replace this old form of shared memory with the new api */
		if (id == 0) {

			//printc("\tCreating shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_ALL_SZ);

			//vkold_shmem_alloc(&vmbooter_shminfo[id], id, COS_SHM_ALL_SZ + ((sizeof(struct cos_shm_rb *)*2)*(VM_COUNT-1)) );
			//for(i = 1; i < (VM_COUNT); i++){
			//	printc("\tInitializing ringbufs for sending\n");
			//	struct cos_shm_rb *sm_rb = NULL;
			//	vk_send_rb_create(sm_rb, i);
			//}

			///* allocating ring buffers for recving data */
			//for(i = 1; i < (VM_COUNT); i++){
			//	printc("\tInitializing ringbufs for rcving\n");
			//	struct cos_shm_rb *sm_rb_r = NULL;
			//	vk_recv_rb_create(sm_rb_r, i);
			//}

		} else {
			//printc("\tMapping shared memory region from %x size %x\n", BOOT_MEM_SHM_BASE, COS_SHM_VM_SZ);
			//vkold_shmem_map(&vmbooter_shminfo[id], id, COS_SHM_VM_SZ);
		}

		printc("\tAllocating/Partitioning Untyped memory\n");
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SZ);

		printc("VM %d Init DONE\n", id);
	}

	printc("------------------[ Booter & VMs init complete ]------------------\n");

	printc("\nRechecking the untyped memory in the booter...\n");
	printc("booter's untyped_frontier is: %p, untyped_ptr is: %p\n", vk_cinfo->mi.untyped_frontier, vk_cinfo->mi.untyped_ptr);
	printc("kernel's untyped_frontier is: %p, untyped_ptr is: %p\n", kernel_cinfo->mi.untyped_frontier, kernel_cinfo->mi.untyped_ptr);

	printc("Starting Timer/Scheduler Thread\n");
	sched_fn(NULL);
	printc("Timer thread DONE\n");

	printc("BACK IN BOOTER COMPONENT, DEAD END!\n");

	return;
}
