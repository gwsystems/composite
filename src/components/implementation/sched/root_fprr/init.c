/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>

u32_t cycs_per_usec = 0;

#define INITIALIZE_PRIO 2
#define INITIALIZE_PERIOD_US (10000)
#define INITIALIZE_BUDGET_US (1000)

#define INF_BUDGET

#define FIXED_PRIO 2
#define FIXED_PERIOD_US (10000)
#ifdef INF_BUDGET
#define FIXED_BUDGET_US (10000)
#else
#define FIXED_BUDGET_US (5000)
#endif

#define CFE_TEST

#define HPET_CORE 0
#define CFE_CORE 0
#define RK_CORE 1

static struct sl_thd *__initializer_thd[NUM_CPU] CACHE_ALIGNED;

static int
schedinit_self(void)
{
	/* if my init is done and i've all child inits */
	if (self_init[cos_cpuid()] && num_child_init[cos_cpuid()] == sched_num_childsched_get()) return 0;

	return 1;
}

static void
__init_done(void *d)
{
	while (schedinit_self()) sl_thd_block_periodic(0);

	PRINTLOG(PRINT_DEBUG, "SELF (inc. CHILD) INIT DONE.\n");

	sl_thd_exit();

	assert(0);
}

void
sched_child_init(struct sched_childinfo *schedci)
{
	struct sl_thd *initthd = NULL;
	int ret;

	assert(schedci);
	initthd = sched_child_initthd_get(schedci);
	assert(initthd);

	if (schedci->flags & COMP_FLAG_SCHED) {
#ifdef INF_BUDGET
		if ((ret = cos_tcap_transfer(sl_thd_rcvcap(initthd), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, FIXED_PRIO))) {
			PRINTC("Failed to transfer INF budget\n");
			assert(0);
		}
#endif
		sl_thd_param_set(initthd, sched_param_pack(SCHEDP_WINDOW, FIXED_PERIOD_US));
		sl_thd_param_set(initthd, sched_param_pack(SCHEDP_BUDGET, FIXED_BUDGET_US));
	}

	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	static volatile int first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };
	int i;
	static volatile int hpet_init_done = NUM_CPU;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	if (ps_cas((unsigned long *)&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		cos_init_args_cpubmp(cpubmp);
	} else {
		while (!ps_load((unsigned long *)&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa((unsigned long *)&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(cpubmp, i)) continue;

		while (!ps_load((unsigned long *)&init_done[i])) ;
	}

#ifdef CFE_TEST
#ifdef CFE_IPI_IF_TEST
	if (cos_cpuid() < 2) /* sending cores starting this number */ {
		sl_init_cpubmp(SL_MIN_PERIOD_US * 10, cpubmp);
	} else {
		sl_init_cpubmp(SL_MIN_PERIOD_US * 100, cpubmp);
	}
#else
	sl_init_cpubmp(SL_MIN_PERIOD_US * 10, cpubmp);
#endif
#else
	sl_init_cpubmp(SL_MIN_PERIOD_US, cpubmp);
#endif
	sched_childinfo_init();

#ifdef CFE_TEST
#ifdef CFE_HPET_IN_ROOTSCHED
	if (cos_cpuid() == HPET_CORE) {
		hpet_ring_init();
		hpet_thd_init();
		hpet_init_done = HPET_CORE;
	}
	if (cos_cpuid() == CFE_CORE) {
		while (hpet_init_done != HPET_CORE) ;
	}
#endif

	if (NUM_CPU > 1 && cos_cpuid() == CFE_CORE) {
		/* wait for RK to initialize */
		while (num_child_init[RK_CORE] == 0) ;
	}
#endif

	if (sched_num_childsched_get() >= 1) {
		__initializer_thd[cos_cpuid()] = sl_thd_alloc(__init_done, NULL);
		assert(__initializer_thd[cos_cpuid()]);
		sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_PRIO, INITIALIZE_PRIO));
		sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_WINDOW, INITIALIZE_BUDGET_US));
		sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_BUDGET, INITIALIZE_PERIOD_US));
	}

	self_init[cos_cpuid()] = 1;
	hypercall_comp_init_done();

#ifdef CFE_TEST
#ifdef CFE_IPI_IF_TEST
	ipi_test_init();
#endif

#ifdef CFE_HPET_IN_ROOTSCHED
	if (cos_cpuid() == HPET_CORE) hpet_attach();
#endif
#endif

	sl_sched_loop_nonblock();

	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}
