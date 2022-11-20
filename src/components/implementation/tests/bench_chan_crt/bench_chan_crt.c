/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <sync_chan.h>
#include <perfdata.h>
#include <cos_time.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

#define ITERATION  10000
/* #define PRINT_ALL */
#define DATA_WORDS 2

thdid_t main_thd = 0, chan_reader = 0, chan_writer = 0;

typedef unsigned int cycles_32_t;

cycles_32_t tmp[DATA_WORDS] = {0, };
cycles_32_t ts1[DATA_WORDS] = {0, };
cycles_32_t ts2[DATA_WORDS] = {0, };
cycles_32_t ts3[DATA_WORDS] = {0, };

struct perfdata perf1, perf2, perf3;
cycles_t result1[ITERATION] = {0, };
cycles_t result2[ITERATION] = {0, };
cycles_t result3[ITERATION] = {0, };

SYNC_CHAN_STATIC_ALLOC(chan0, cycles_32_t, DATA_WORDS);
SYNC_CHAN_STATIC_ALLOC(chan1, cycles_32_t, DATA_WORDS);
SYNC_CHAN_STATIC_ALLOC(chan2, cycles_32_t, DATA_WORDS);
SYNC_CHAN_STATIC_ALLOC(chan3, cycles_32_t, DATA_WORDS);
SYNC_CHAN_STATIC_ALLOC(chan4, cycles_32_t, DATA_WORDS);
SYNC_CHAN_TYPE_PROTOTYPES(bench, cycles_32_t, DATA_WORDS);

struct bench_args {
	struct sync_chan *r, *w;
} args[4];

/*
 * The two threads reciprocally sends and receives.
 */
void
chan_reader_thd(void *d)
{
	struct bench_args *cs = d;
	assert(cs);

	/* Never stops running; writer controls how many iters to run. */
	while(1) {
		debug("r1,");
		sync_chan_recv_bench(cs->r /*chan1*/, tmp);
		debug("tsr1: %d,", tmp[0]);
		debug("r2,");
		tmp[0] = time_now();
		debug("tsr2: %d,", tmp[0]);
		debug("r3,");
		sync_chan_send_bench(/*chan2*/ cs->w, tmp);
		debug("r4,");
	}
}

void
chan_writer_thd(void *d)
{
	int i;
	int first = 0;
	struct bench_args *cs = d;
	assert(cs);

	for (int i = 0; i < ITERATION + 1; i++) {
		debug("w1,");
		ts1[0] = time_now();
		debug("ts1: %d,", ts1[0]);
		debug("w2,");
		sync_chan_send_bench(/*chan1*/cs->w, ts1);
		debug("w3,");
		sync_chan_recv_bench(/*chan2*/cs->r, ts2);
		debug("ts2: %d,", ts2[0]);
		debug("w4,");
		ts3[0] = time_now();
		debug("w5,");

		if (first == 0) {
			first = 1;
		} else if (ts2[0] > ts1[0] && ts3[0] > ts2[0]) {
			perfdata_add(&perf1, ts2[0] - ts1[0]);
			perfdata_add(&perf2, ts3[0] - ts2[0]);
			perfdata_add(&perf3, ts3[0] - ts1[0]);
		}
	}

	perfdata_calc(&perf1);
	perfdata_calc(&perf2);
	perfdata_calc(&perf3);
#ifdef PRINT_ALL
	perfdata_all(&perf1);
	perfdata_all(&perf2);
	perfdata_all(&perf3);
#else
	perfdata_print(&perf1);
	perfdata_print(&perf2);
	perfdata_print(&perf3);
#endif

	sched_thd_wakeup(main_thd);
	sched_thd_block(0);
	BUG();
}

void
test_chan(void)
{
	int i;
	int first = 0;
	cycles_t begin, end;
	struct bench_args _args[] = {
		{.r = chan1, .w = chan2},
		{.r = chan2, .w = chan1},
		{.r = chan3, .w = chan4},
		{.r = chan4, .w = chan3}
	};
	for (i = 0; i < 4; i++) {
		args[i] = _args[i];
	}

	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4)
	};

	sync_chan_init_bench(chan0);

	/*
	 * Test the uncontended channel.
	 */
	perfdata_init(&perf1, "Uncontended channel - selfloop", result1, ITERATION);
	for (i = 0; i < ITERATION + 1; i++) {
		begin = time_now();

		sync_chan_send_bench(chan0, tmp);
		sync_chan_recv_bench(chan0, tmp);

		end = time_now();
		if (first == 0) first = 1;
		else            perfdata_add(&perf1, end - begin);
	}
	perfdata_calc(&perf1);
#ifdef PRINT_ALL
	perfdata_all(&perf1);
#else
	perfdata_print(&perf1);
#endif

	sync_chan_init_bench(chan1);
	sync_chan_init_bench(chan2);
	sync_chan_init_bench(chan3);
	sync_chan_init_bench(chan4);

	main_thd = cos_thdid();

	/*
	 * Test from a low to a high thread.
	 */
	perfdata_init(&perf1, "Contended channel - low writer -> high reader", result1, ITERATION);
	perfdata_init(&perf2, "Contended channel - high reader -> low writer", result2, ITERATION);
	perfdata_init(&perf3, "Contended channel - round trip", result3, ITERATION);

	printc("Create threads:\n");

	chan_reader = sched_thd_create(chan_reader_thd, &args[0]);
	printc("\tcreating reader thread %lu at prio %d\n", chan_reader, sps[0]);
	sched_thd_param_set(chan_reader, sps[0]);

	chan_writer = sched_thd_create(chan_writer_thd, &args[1]);
	printc("\tcreating writer thread %lu at prio %d\n", chan_writer, sps[1]);
	sched_thd_param_set(chan_writer, sps[1]);

	sched_thd_block(0);

	/*
	 * Test from a high to a low thread.
	 */
	perfdata_init(&perf1, "Contended channel - high writer -> low reader", result1, ITERATION);
	perfdata_init(&perf2, "Contended channel - low reader -> high writer", result2, ITERATION);
	perfdata_init(&perf3, "Contended channel - round trip", result3, ITERATION);

	chan_reader = sched_thd_create(chan_reader_thd, &args[2]);
	printc("\tcreating reader thread %lu at prio %d\n", chan_reader, sps[2]);
	sched_thd_param_set(chan_reader, sps[2]);

	chan_writer = sched_thd_create(chan_writer_thd, &args[3]);
	printc("\tcreating writer thread %lu at prio %d\n", chan_writer, sps[3]);
	sched_thd_param_set(chan_writer, sps[3]);

	sched_thd_block(0);

	printc("SUCCESS: channel benchmark complete.\n");
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
