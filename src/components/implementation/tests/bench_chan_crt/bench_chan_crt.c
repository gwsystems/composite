/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <crt_static_chan.h>
#include <perfdata.h>
#include <cos_time.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* One low-priority thread and one high-priority thread contends on the lock */
#define ITERATION 10000
/* #define PRINT_ALL */

/* Two options are available: Sender at low/high prio, data words 4 */
#define READER_HIGH
#define DATA_WORDS		2

thdid_t chan_reader = 0, chan_writer = 0;

typedef unsigned int cycles_32_t;

cycles_32_t tmp[DATA_WORDS] = {0, };
cycles_32_t ts1[DATA_WORDS] = {0, };
cycles_32_t ts2[DATA_WORDS] = {0, };
cycles_32_t ts3[DATA_WORDS] = {0, };

struct perfdata perf1, perf2, perf3;
cycles_t result1[ITERATION] = {0, };
cycles_t result2[ITERATION] = {0, };
cycles_t result3[ITERATION] = {0, };

CRT_STATIC_CHAN_STATIC_ALLOC(chan1, cycles_32_t, DATA_WORDS);
CRT_STATIC_CHAN_STATIC_ALLOC(chan2, cycles_32_t, DATA_WORDS);
CRT_STATIC_CHAN_TYPE_PROTOTYPES(bench, cycles_32_t, DATA_WORDS);

/***
 * The two threads reciprocally sends and receives.
 */
void
chan_reader_thd(void *d)
{
	/* Never stops running; writer controls how many iters to run. */
	while(1) {
		debug("r1,");
		crt_static_chan_recv_bench(chan1, tmp);
		debug("tsr1: %d,", tmp[0]);
		debug("r2,");
		tmp[0] = time_now();
		debug("tsr2: %d,", tmp[0]);
		debug("r3,");
		crt_static_chan_send_bench(chan2, tmp);
		debug("r4,");
	}
}

void
chan_writer_thd(void *d)
{
	int i;
	int first = 0;

	for (int i = 0; i < ITERATION + 1; i++) {
		debug("w1,");
		ts1[0] = time_now();
		debug("ts1: %d,", ts1[0]);
		debug("w2,");
		crt_static_chan_send_bench(chan1, ts1);
		debug("w3,");
		crt_static_chan_recv_bench(chan2, ts2);
		debug("ts2: %d,", ts2[0]);
		debug("w4,");
		ts3[0] = time_now();
		debug("w5,");

		if (first == 0) first = 1;
		else {
			if (ts2[0] > ts1[0] && ts3[0] > ts2[0]) {
				perfdata_add(&perf1, ts2[0] - ts1[0]);
				perfdata_add(&perf2, ts3[0] - ts2[0]);
				perfdata_add(&perf3, ts3[0] - ts1[0]);
			}
		}
	}

	perfdata_calc(&perf1);
	perfdata_calc(&perf2);
	perfdata_calc(&perf3);
#ifdef PRINT_ALL
#ifdef READER_HIGH
	perfdata_all(&perf1);
#else
	perfdata_all(&perf2);
#endif
	perfdata_all(&perf3);
#else
#ifdef READER_HIGH
	perfdata_print(&perf1);
#else
	perfdata_print(&perf2);
#endif
	perfdata_print(&perf3);
#endif

	while (1) ;
}

void
test_chan(void)
{
	int i;
	int first = 0;
	cycles_t begin, end;

#ifdef READER_HIGH
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};
#else
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4)
	};
#endif

	crt_static_chan_init_bench(chan1);

	/* Uncontended lock taking/releasing */
	perfdata_init(&perf1, "Uncontended channel - selfloop", result1, ITERATION);
	for (i = 0; i < ITERATION + 1; i++) {
		begin = time_now();

		crt_static_chan_send_bench(chan1, tmp);
		crt_static_chan_recv_bench(chan1, tmp);

		end = time_now();
		if (first == 0) first = 1;
		else perfdata_add(&perf1, end - begin);
	}
	perfdata_calc(&perf1);
#ifdef PRINT_ALL
	perfdata_all(&perf1);
#else
	perfdata_print(&perf1);
#endif

	crt_static_chan_init_bench(chan1);
	crt_static_chan_init_bench(chan2);

	perfdata_init(&perf1, "Contended channel - reader high use this", result1, ITERATION);
	perfdata_init(&perf2, "Contended channel - writer high use this", result2, ITERATION);
	perfdata_init(&perf3, "Contended channel - roundtrip", result3, ITERATION);

	printc("Create threads:\n");

	chan_reader = sched_thd_create(chan_reader_thd, NULL);
	printc("\tcreating reader thread %lu at prio %d\n", chan_reader, sps[0]);
	sched_thd_param_set(chan_reader, sps[0]);

	chan_writer = sched_thd_create(chan_writer_thd, NULL);
	printc("\tcreating writer thread %lu at prio %d\n", chan_writer, sps[1]);
	sched_thd_param_set(chan_writer, sps[1]);
}

void
cos_init(void)
{
	printc("Benchmark for the crt_chan (w/sched interface).\n");
}

int
main(void)
{
	test_chan();

	printc("Running benchmark, exiting main thread...\n");

	return 0;
}
