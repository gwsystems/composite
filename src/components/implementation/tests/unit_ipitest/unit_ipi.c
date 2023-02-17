/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <sched.h>

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

#define TEST_WAKEUP_CORE 0
#define TEST_RCV_CORE 1

static volatile int test_done = 0;
static volatile thdid_t thd[2] = { 0 };

static void
ipi_wakeup()
{
	while (!thd[1]) ;
	sched_thd_wakeup(thd[1]);
	SPIN();
	return;
}

static void
ipi_blocked()
{
	while (!thd[0]) ;
	sched_thd_block(0);
	printc("xcpu wakeup SUCCESS.\n");
	printc("SPIN...\n");
	SPIN();
	return;
}

void
test_ipi_switch(void)
{
	if (cos_cpuid() == TEST_WAKEUP_CORE) {
		printc("Testing ipi xcore wakeup\n");

		thd[0] = sched_thd_create(ipi_wakeup, NULL);
		sched_thd_param_set(thd[0], sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

		sched_thd_yield_to(thd[0]);
	}
	if (cos_cpuid() == TEST_RCV_CORE){
		thd[1] = sched_thd_create(ipi_blocked, NULL);
		sched_thd_param_set(thd[1], sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));
		
		sched_thd_yield_to(thd[1]);
	}
	SPIN();
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{
	int i = 0;

	if (NUM_CPU < 2) {
		printc("ERROR: This test requires at least 2 cores.\n");
		return;
	}
	test_ipi_switch();
	SPIN();
	return;
}
