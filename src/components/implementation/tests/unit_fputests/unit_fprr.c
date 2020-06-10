/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_defkernel_api.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>

/* Ensure this is the same as what is in sl_mod_fprr.c */
#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

static int lowest_was_scheduled[NUM_CPU] = { 0 };
static int high_thd_test_status[NUM_CPU] = { 0 }; /* 1 = failure */

static void
low_thread_fn()
{
	lowest_was_scheduled[cos_cpuid()] = 1;
	sl_thd_exit();
}

static void
high_thread_fn()
{
	struct sl_thd *low_thread;
	cycles_t deadline;

	low_thread = sl_thd_alloc(low_thread_fn, NULL);
	sl_thd_param_set(low_thread, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	deadline = sl_now() + sl_usec2cyc(10 * 1000 * 1000);
	while (sl_now() < deadline) {}
	if (lowest_was_scheduled[cos_cpuid()]) high_thd_test_status[cos_cpuid()] = 1;
	sl_thd_exit();
}

static void
test_highest_is_scheduled(void)
{
	struct sl_thd *high_thread;
	cycles_t wakeup;

	high_thread = sl_thd_alloc(high_thread_fn, NULL);
	sl_thd_param_set(high_thread, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = sl_now() + sl_usec2cyc(1000 * 1000);
	sl_thd_block_timeout(0, wakeup);
}

static int thd1_ran[NUM_CPU] = { 0 };
static int thd2_ran[NUM_CPU] = { 0 };

static void
thd1_fn()
{
	thd1_ran[cos_cpuid()] = 1;
	while (1);
}

static void
thd2_fn()
{
	thd2_ran[cos_cpuid()] = 1;
	while (1);
}

static void
allocator_thread_fn()
{
	struct sl_thd *thd1, *thd2;
	cycles_t wakeup;

	thd1 = sl_thd_alloc(thd1_fn, NULL);
	sl_thd_param_set(thd1, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	thd2 = sl_thd_alloc(thd2_fn, NULL);
	sl_thd_param_set(thd2, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	wakeup = sl_now() + sl_usec2cyc(1000 * 1000);
	sl_thd_block_timeout(0, wakeup);

	sl_thd_free(thd1);
	sl_thd_free(thd2);

	sl_thd_exit();
}

static void
test_swapping(void)
{
	struct sl_thd *allocator_thread;
	cycles_t wakeup;

	allocator_thread = sl_thd_alloc(allocator_thread_fn, NULL);
	sl_thd_param_set(allocator_thread, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = sl_now() + sl_usec2cyc(100 * 1000);
	sl_thd_block_timeout(0, wakeup);
}

#define XCPU_THDS (NUM_CPU-1)
#define THD_SLEEP_US (100 * 1000)
volatile unsigned int xcpu_thd_data[NUM_CPU][XCPU_THDS];
volatile unsigned int xcpu_thd_counter[NUM_CPU];
static void
test_xcpu_fn(void *data)
{
	cycles_t wakeup, elapsed;
	int cpu = *((unsigned int *)data) >> 16;
	int i   = (*((unsigned int *)data) << 16) >> 16;

	assert(i < XCPU_THDS);
	wakeup = sl_now() + sl_usec2cyc(THD_SLEEP_US);
	elapsed = sl_thd_block_timeout(0, wakeup);

	if (elapsed) xcpu_thd_counter[cpu] ++;
	sl_thd_exit();
}

static void
run_xcpu_tests()
{
	int ret = 0, i, cpu = 0;

	if (NUM_CPU == 1) return;

	memset((void *)xcpu_thd_data[cos_cpuid()], 0, sizeof(unsigned int) * XCPU_THDS);
	xcpu_thd_counter[cos_cpuid()] = 0;

	for (i = 0; i < XCPU_THDS; i++) {
		sched_param_t p[1];

		if (cpu == cos_cpuid()) cpu++;
		cpu %= NUM_CPU;
		xcpu_thd_data[cos_cpuid()][i] = (cpu << 16) | i;

		p[0] = sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY);
		ret = sl_xcpu_thd_alloc(cpu, test_xcpu_fn, (void *)&xcpu_thd_data[cos_cpuid()][i], p);
		if (ret) break;

		cpu++;
	}

	PRINTC("%s: Creating cross-CPU threads!\n", ret ? "FAILURE" : "SUCCESS");
	while (xcpu_thd_counter[cos_cpuid()] != XCPU_THDS) ;
}

static void
run_tests()
{
	test_highest_is_scheduled();
	PRINTC("%s: Schedule highest priority thread only!\n", high_thd_test_status[cos_cpuid()] ? "FAILURE" : "SUCCESS");
	test_swapping();
	PRINTC("%s: Swap back and forth!\n", (thd1_ran[cos_cpuid()] && thd2_ran[cos_cpuid()]) ? "SUCCESS" : "FAILURE");

	run_xcpu_tests();

	PRINTC("Unit-test done!\n");
	sl_thd_exit();
}

void
cos_init(void)
{
	int i;
	static unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	struct sl_thd *testing_thread;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	PRINTC("Unit-test for the scheduling library (sl)\n");

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa(&init_done[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}

	sl_init(SL_MIN_PERIOD_US);

	testing_thread = sl_thd_alloc(run_tests, NULL);
	sl_thd_param_set(testing_thread, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

	sl_sched_loop();

	assert(0);

	return;
}
