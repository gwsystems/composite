/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>
#include <perfdata.h>
#include <cos_time.h>

#undef YIELD_TRACE_DEBUG
#ifdef YIELD_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* lo and hi is actually running at the same prio */
#define ITERATION 10000
/* #define PRINT_ALL */

thdid_t yield_hi = 0, yield_lo = 0;
volatile cycles_t start;
volatile cycles_t end;

struct perfdata perf;
cycles_t result[ITERATION] = {0, };

/***
 * We're measuring 2-way context switch time.
 */
void
yield_hi_thd(void *d)
{
	/* Never stops running; low priority controls how many iters to run. */
	while (1) {
		debug("h1,");
		sched_thd_yield_to(yield_lo);
		debug("h2,");
	}
}

void
yield_lo_thd(void *d)
{
	int i;
	int first = 0;

	for (i = 0; i < ITERATION + 1; i++) {
		debug("l1,");

		start = time_now();
		sched_thd_yield_to(yield_hi);
		end = time_now();

		debug("l2,");

		if (first == 0) first = 1;
		else perfdata_add(&perf, end - start);
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
test_yield(void)
{
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};

	perfdata_init(&perf, "Context switch time", result, ITERATION);

	printc("Create threads:\n");

	yield_lo = sched_thd_create(yield_lo_thd, NULL);
	printc("\tcreating lo thread %ld at prio %d\n", yield_lo, sps[1]);
	sched_thd_param_set(yield_lo, sps[1]);

	yield_hi = sched_thd_create(yield_hi_thd, NULL);
	printc("\tcreating hi thread %ld at prio %d\n", yield_hi, sps[0]);
	sched_thd_param_set(yield_hi, sps[0]);
}

void
cos_init(void)
{
	printc("Benchmark for the sched_thd_yield (w/sched interface).\n");
}

int
main(void)
{
	test_yield();

	printc("Running benchmark, exiting main thread...\n");

	return 0;
}
