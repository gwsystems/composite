/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <sched.h>
#include <cos_time.h>

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

static int lowest_was_scheduled[NUM_CPU];

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

// Cathal's new mess
#include <capmgr.h>

volatile unsigned long *rdy = NULL;
#define MAX_USE_PIPE_SZ 1
#define SND_DATA 0x4321
#define HPET_PERIOD_TEST_US 20000

int iters = 0;
#define ITERS 100000
cycles_t vals[ITERS] = { 0 };
static cycles_t *sttsc = vals;


static void
_test_hw_attach(arcvcap_t* rcv)
{
	int a = capmgr_hw_periodic_attach(HW_PERIODIC, *rcv, HPET_PERIOD_TEST_US);
	/* TODO: register to HPET */
	while (1) {
		int ret = cos_rcv(*rcv, 0, 0); //added 0 for no flangs, should 
		printc("### 5 ### RET: %d\n", ret);
		iters++;
		rdtscll(*sttsc);
		//chan_out(SND_DATA)
		if (iters == 10) capmgr_hw_detach(HW_PERIODIC);
	}

	return;
}

struct cos_aep_info intaep;
#define SPDID_INT 5

static void
test_aeps(void)
{
	thdid_t tid;
	int ret;
	int i = 0;

	if (cos_spd_id() == SPDID_INT) {
		tid = sched_aep_create(&intaep, _test_hw_attach, &intaep.rcv, 0, 0, 0, 0);
		printc("### 1 #### AEP thd created thdid: %lu, rcv:%d \n", tid, intaep.rcv);
		sched_thd_param_set(tid, sched_param_pack(SCHEDP_PRIO, 1));
	} else {
		//removed, as want to try force the hw attach to be tried
		// tid = sched_thd_create(__test_wrk_fn, 
		// 	((cos_spd_id() == SPDID_W3 && MAX_USE_PIPE_SZ == 4) 
		// 	|| (cos_spd_id() == SPDID_W1 && MAX_USE_PIPE_SZ == 2)) 
		// 	? (void *)1: (void *)0);
		printc("periodic timer hw attach not run \n");
	}
	assert(tid);
}

// end cathal's new mess

static void
run_tests()
{
	test_highest_is_scheduled();
	PRINTLOG(PRINT_DEBUG, "Test successful! Highest was scheduled only!\n");
	test_swapping();
	PRINTLOG(PRINT_DEBUG, "Test successful! We swapped back and forth!\n");
	test_aeps();
	PRINTLOG(PRINT_DEBUG, "Done testing, spinning...\n");
	SPIN();
}

int
main(void)
{
	thdid_t testtid;

	PRINTLOG(PRINT_DEBUG, "Unit-test scheduling manager component\n");

	testtid = sched_thd_create(run_tests, NULL);
	sched_thd_param_set(testtid, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

	while (1) {
		cycles_t wakeup;

		wakeup = time_now() + time_usec2cyc(1000 * 1000);
		sched_thd_block_timeout(0, wakeup);
	}

	return 0;
}
