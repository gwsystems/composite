/*
 * Copyright 2020, Bite Ye, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <patina.h>
#include <perfdata.h>

#undef TMR_TRACE_DEBUG
#ifdef TMR_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* High-priority thread interrupts the low-priority thread by timer ticks */
#define ITERATION 10 * 1000
#define TMR_PERIODIC_TIME 10 * 1000
#define DROP_THRESHOLD 0x1000000U

#define PRINT_ALL

thdid_t tmr_hi = 0, tmr_lo = 0;

typedef unsigned int cycles_32_t;
volatile cycles_32_t start;
volatile cycles_32_t end;

struct perfdata perf;
cycles_t        result[ITERATION] = {
  0,
};

/***
 * The high priority thread sets up a periodic timer while the low priority thread keeps looping and updating
 * the timing value variable. The variable is a 32-bit one so that it can be updated atomically. We always
 * drop values larger than DROP_THRESHOLD (clearly impossible; appears due to counter overflow).
 */
void
tmr_hi_thd(void *d)
{
	int            i;
	patina_time_t  t;
	patina_timer_t tid;
	patina_event_t evt;
	int            first = 0;

	printc("Call into timer manager to make a timer.\n");
	tid = patina_timer_create();

	printc("Call into event manager to make a event.\n");
	patina_event_create(&evt, 1);

	/*
	 * Add the timer event to the event set, the associate the timer with that event ID so
	 * the timer manager knows which event to trigger when the timer expires.
	 */
	patina_event_add(&evt, tid, 0);

	/* Start the timer */
	patina_time_create(&t, 0, TMR_PERIODIC_TIME);
	patina_timer_periodic(tid, NULL, &t);

	/* Event loop */
	i = 0;
	while (i < ITERATION + 1) {
		patina_event_wait(&evt, NULL, 0);
		end = (cycles_32_t)time_now();

		if ((end - start) > DROP_THRESHOLD) continue;

		if (first == 0)
			first = 1;
		else
			perfdata_add(&perf, end - start);
		debug("%lld.\n", end - start);

		i++;
	}

	perfdata_calc(&perf);
#ifdef PRINT_ALL
	perfdata_all(&perf);
#else
	perfdata_print(&perf);
#endif

	while (1)
		;
}

void
tmr_lo_thd(void *d)
{
	/* Runs indefinitely */
	while (1) { start = (cycles_32_t)time_now(); }
}

void
test_tmr(void)
{
	int i;

	sched_param_t sps[] = {SCHED_PARAM_CONS(SCHEDP_PRIO, 4), SCHED_PARAM_CONS(SCHEDP_PRIO, 6)};


	perfdata_init(&perf, "Timer latency - total", result, ITERATION);

	printc("Create threads:\n");

	tmr_lo = sched_thd_create(tmr_lo_thd, NULL);
	printc("\tcreating lo thread %d at prio %d\n", tmr_lo, sps[1]);
	sched_thd_param_set(tmr_lo, sps[1]);

	tmr_hi = sched_thd_create(tmr_hi_thd, NULL);
	printc("\tcreating hi thread %d at prio %d\n", tmr_hi, sps[0]);
	sched_thd_param_set(tmr_hi, sps[0]);
}

void
cos_init(void)
{
	printc("Benchmark for the tmrmgr (w/sched & evt interface).\n");
}

int
main(void)
{
	sched_thd_block_timeout(0, time_now() + time_usec2cyc(1000 * 1000));

	test_tmr();

	printc("Running benchmark, exiting main thread...\n");

	return 0;
}