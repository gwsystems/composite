/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched.h>
#include <cos_time.h>

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

static int lowest_was_scheduled[NUM_CPU];
static u32_t cycs_per_usec = 0;

static void
low_thread_fn()
{
	lowest_was_scheduled[cos_cpuid()] = 1;
	sched_thd_exit();
}

static void
high_thread_fn()
{
	thdid_t lowtid;
	cycles_t deadline;

	lowtid = sched_thd_create(low_thread_fn, NULL);
	sched_thd_param_set(lowtid, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	deadline = time_now() + time_usec2cyc(10 * 1000 * 1000);
	while (time_now() < deadline) {}
	assert(!lowest_was_scheduled[cos_cpuid()]);
	sched_thd_exit();
}

static void
test_highest_is_scheduled(void)
{
	thdid_t hitid;
	cycles_t wakeup;

	hitid = sched_thd_create(high_thread_fn, NULL);
	sched_thd_param_set(hitid, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = time_now() + time_usec2cyc(1000 * 1000);
	sched_thd_block_timeout(0, wakeup);
}

static int thd1_ran[NUM_CPU];
static int thd2_ran[NUM_CPU];

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
	thdid_t tid1, tid2;
	cycles_t wakeup;

	tid1 = sched_thd_create(thd1_fn, NULL);
	sched_thd_param_set(tid1, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	tid2 = sched_thd_create(thd2_fn, NULL);
	sched_thd_param_set(tid2, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	wakeup = time_now() + time_usec2cyc(1000 * 1000);
	sched_thd_block_timeout(0, wakeup);

	sched_thd_delete(tid1);
	sched_thd_delete(tid2);

	sched_thd_exit();
}

static void
test_swapping(void)
{
	thdid_t alloctid;
	cycles_t wakeup;

	alloctid = sched_thd_create(allocator_thread_fn, NULL);
	sched_thd_param_set(alloctid, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = time_now() + time_usec2cyc(100 * 1000);
	sched_thd_block_timeout(0, wakeup);
}

static void
run_tests()
{
	test_highest_is_scheduled();
	PRINTLOG(PRINT_DEBUG, "Test successful! Highest was scheduled only!\n");
	test_swapping();
	PRINTLOG(PRINT_DEBUG, "Test successful! We swapped back and forth!\n");

	PRINTLOG(PRINT_DEBUG, "Done testing, spinning...\n");
	SPIN();
}

void
cos_init(void)
{
	thdid_t testtid;
	spdid_t child;
	comp_flag_t childflag;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	PRINTLOG(PRINT_DEBUG, "Unit-test scheduling manager component\n");
	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	testtid = sched_thd_create(run_tests, NULL);
	sched_thd_param_set(testtid, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

	while (1) {
		cycles_t wakeup;

		wakeup = time_now() + time_usec2cyc(1000 * 1000);
		sched_thd_block_timeout(0, wakeup);
	}

	/* should never get here */
	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
