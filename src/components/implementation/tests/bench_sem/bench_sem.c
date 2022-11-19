/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <sync_sem.h>
#include <perfdata.h>
#include <cos_time.h>

#undef SEM_TRACE_DEBUG
#ifdef SEM_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* One low-priority thread and one high-priority thread contends on the semaphore */
#define ITERATION 100
//#define PRINT_ALL

struct sync_sem sem;
thdid_t sem_hi = 0, sem_lo = 0;
volatile int flag = 0;

volatile cycles_t start;
volatile cycles_t end;

struct perfdata perf;
cycles_t result[ITERATION] = {0, };

/***
 * The high priority thread periodically challenges the sem while the low priority thread keeps spinning.
 * When the low-priority thread detects that the flag is changed, it knows that the sem is challenged.
 * Execution: hi take -> lo release -> hi release, there is 1 contended take, 1 give, and 2 ctxsws.
 */
void
sem_hi_thd(void *d)
{
	/* Never stops running; low priority controls how many iters to run. */
	while (1) {
		debug("h1");
		sched_thd_block(0);
		sched_thd_block_timeout(0, time_now() + time_usec2cyc(1000));

		debug("h2");
		flag = 1;
		start = time_now();

		sync_sem_take(&sem);
		sync_sem_give(&sem);

		end = time_now();
		debug("h3");
	}
}

void
sem_lo_thd(void *d)
{
	int i;
	int first = 0;

	for (i = 0; i < ITERATION + 1; i++) {
		debug("l1");
		sched_thd_wakeup(sem_hi);

		debug("l2");
		flag = 0;
		sync_sem_take(&sem);

		debug("l3");
		while (flag != 1) {}

		sync_sem_give(&sem);

		if (first == 0) first = 1;
		else perfdata_add(&perf, end - start);
		debug("l4");
	}

	perfdata_calc(&perf);
#ifdef PRINT_ALL
	perfdata_all(&perf);
#else
	perfdata_print(&perf);
#endif

	while (1) ;
}

void
test_sem(void)
{
	int i;
	int first = 0;
	cycles_t start, end;

	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};

	sync_sem_init(&sem, 1);

	/* Uncontended semaphore taking/releasing */
	perfdata_init(&perf, "Uncontended semaphore - take+give", result, ITERATION);
	for (i = 0; i < ITERATION + 1; i++) {
		start = time_now();

		sync_sem_take(&sem);
		sync_sem_give(&sem);

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

	perfdata_init(&perf, "Contended semaphore - take+give", result, ITERATION);

	printc("Create threads:\n");

	sem_lo = sched_thd_create(sem_lo_thd, NULL);
	printc("\tcreating lo thread %ld at prio %d\n", sem_lo, sps[1]);
	sched_thd_param_set(sem_lo, sps[1]);

	sem_hi = sched_thd_create(sem_hi_thd, NULL);
	printc("\tcreating hi thread %ld at prio %d\n", sem_hi, sps[0]);
	sched_thd_param_set(sem_hi, sps[0]);
}

void
cos_init(void)
{
	printc("Benchmark for the sync_sem (w/sched interface).\n");
}

int
main(void)
{
	test_sem();

	printc("Running benchmark, exiting main thread...\n");
	sched_thd_block(0);
	BUG();

	return 0;
}
