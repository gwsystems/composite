/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <llboot.h>
#include <schedmgr.h>

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

static int lowest_was_scheduled = 0;
static u32_t cycs_per_usec = 0;

static void
low_thread_fn()
{
	lowest_was_scheduled = 1;
	schedmgr_thd_exit(0);
}

static cycles_t
now(void)
{
	cycles_t nowc;

	rdtscll(nowc);

	return nowc;
}

static u64_t
usec2cyc(cycles_t usec)
{
	return usec * cycs_per_usec;
}

static u64_t
cyc2usec(cycles_t cyc)
{
	return cyc / cycs_per_usec;
}

static void
high_thread_fn()
{
	thdid_t lowtid;
	cycles_t deadline;

	lowtid = schedmgr_thd_create(0, low_thread_fn, NULL);
	schedmgr_thd_param_set(0, lowtid, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	deadline = now() + usec2cyc(10 * 1000 * 1000);
	while (now() < deadline) {}
	assert(!lowest_was_scheduled);
	schedmgr_thd_exit(0);
}

static void
test_highest_is_scheduled(void)
{
	thdid_t hitid;
	cycles_t wakeup;

	hitid = schedmgr_thd_create(0, high_thread_fn, NULL);
	schedmgr_thd_param_set(0, hitid, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = now() + usec2cyc(1000 * 1000);
	schedmgr_thd_block_timeout(0, 0, wakeup);
}

static int thd1_ran = 0;
static int thd2_ran = 0;

static void
thd1_fn()
{
	thd1_ran = 1;
	while (1);
}

static void
thd2_fn()
{
	thd2_ran = 1;
	while (1);
}

static void
allocator_thread_fn()
{
	thdid_t tid1, tid2;
	cycles_t wakeup;

	tid1 = schedmgr_thd_create(0, thd1_fn, NULL);
	schedmgr_thd_param_set(0, tid1, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	tid2 = schedmgr_thd_create(0, thd2_fn, NULL);
	schedmgr_thd_param_set(0, tid2, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	wakeup = now() + usec2cyc(1000 * 1000);
	schedmgr_thd_block_timeout(0, 0, wakeup);

	schedmgr_thd_delete(0, tid1);
	schedmgr_thd_delete(0, tid2);

	schedmgr_thd_exit(0);
}

static void
test_swapping(void)
{
	thdid_t alloctid;
	cycles_t wakeup;

	alloctid = schedmgr_thd_create(0, allocator_thread_fn, NULL);
	schedmgr_thd_param_set(0, alloctid, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = now() + usec2cyc(100 * 1000);
	schedmgr_thd_block_timeout(0, 0, wakeup);
}

static void
run_tests()
{
	test_highest_is_scheduled();
	printc("Test successful! Highest was scheduled only!\n");
	test_swapping();
	printc("Test successful! We swapped back and forth!\n");

	printc("Done testing, spinning...\n");
	SPIN();
}

void
cos_init(void)
{
	thdid_t testtid;
	u64_t childbits = 0;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	printc("Unit-test scheduling component\n");
	llboot_comp_childspdids_get(&childbits);
	assert(!childbits);

	testtid = schedmgr_thd_create(0, run_tests, NULL);
	schedmgr_thd_param_set(0, testtid, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

	while (1) {
		cycles_t wakeup;

		wakeup = now() + usec2cyc(1000 * 1000);
		schedmgr_thd_block_timeout(0, 0, wakeup);
	}
	return;
}
