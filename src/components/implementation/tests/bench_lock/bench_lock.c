/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <sync_lock.h>
#include <perfdata.h>
#include <cos_time.h>

#undef LOCK_TRACE_DEBUG
#ifdef LOCK_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* One low-priority thread and one high-priority thread contends on the lock */
#define ITERATION 200
/* #define PRINT_ALL */

struct sync_lock lock;
thdid_t lock_hi = 0, lock_lo = 0;
volatile int flag = 0;

volatile cycles_t start;
volatile cycles_t end;

struct perfdata perf;
cycles_t result[ITERATION] = {0, };

/***
 * The high priority thread periodically challenges the lock while the low priority thread keeps spinning.
 * When the low-priority thread detects that the flag is changed, it knows that the lock is challenged.
 * Execution: hi take -> lo release -> hi release, there is 1 contended take, 1 release, and 2 ctxsws.
 */
void
lock_hi_thd(void *d)
{
	/* Never stops running; low priority controls how many iters to run. */
	while (1) {
		debug("h1,");
		//sched_thd_block(0);
		sched_thd_block_timeout(0, time_now() + time_usec2cyc(1000));

		debug("h2,");
		flag = 1;
		start = time_now();
		sync_lock_take(&lock);

		sync_lock_release(&lock);
		end = time_now();
		debug("h3,");
	}
}

void
lock_lo_thd(void *d)
{
	int i;
	int first = 0;

	for (i = 0; i < ITERATION + 1; i++) {
		debug("l1,");
		//sched_thd_wakeup(lock_hi);

		debug("l2,");
		//flag = 0;
		sync_lock_take(&lock);

		debug("l3,");
		while (flag != 1) ;
		flag = 0;

		sync_lock_release(&lock);

		if (first == 0) first = 1;
		else perfdata_add(&perf, end - start);
		debug("l4,");
	}

	perfdata_calc(&perf);
#ifdef PRINT_ALL
	perfdata_all(&perf);
#else
	perfdata_print(&perf);
#endif

	printc("SUCCESS: Finished lock tests.\n");
	while (1) ;
}

void
test_lock(void)
{
	int i;
	int first = 0;

	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};

	sync_lock_init(&lock);

	/* Uncontended lock taking/releasing */
	perfdata_init(&perf, "Uncontended lock - take+release", result, ITERATION);
	for (i = 0; i < ITERATION + 1; i++) {
		start = time_now();

		sync_lock_take(&lock);
		sync_lock_release(&lock);

		end = time_now();
		if (first == 0) first = 1;
		else perfdata_add(&perf, end - start);
	}
	perfdata_calc(&perf);
#ifdef PRINT_ALL
	perfdata_all(&perf);
#else
	perfdata_print(&perf);
#endif

	perfdata_init(&perf, "Contended lock - take+release", result, ITERATION);

	printc("Create threads:\n");

	lock_lo = sched_thd_create(lock_lo_thd, NULL);
	printc("\tcreating lo thread %lu at prio %d\n", lock_lo, sps[1]);
	sched_thd_param_set(lock_lo, sps[1]);

	lock_hi = sched_thd_create(lock_hi_thd, NULL);
	printc("\tcreating hi thread %lu at prio %d\n", lock_hi, sps[0]);
	sched_thd_param_set(lock_hi, sps[0]);
}

void
cos_init(void)
{
	printc("Benchmark for the sync_lock (w/sched interface).\n");
}

int
main(void)
{
	test_lock();

	printc("Running benchmark, exiting main thread...\n");

	return 0;
}
