#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>
#include <perfdata.h>
#include <cos_time.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

struct chan sc, rc;
struct chan_snd init_s;
struct chan_snd s;
struct chan_rcv r;
struct evt e;

#define ITERATION 	10000
#define READER_HIGH
#define USE_EVTMGR
/* #define PRINT_ALL */

#define TEST_CHAN_ITEM_SZ   sizeof(u32_t)
#define TEST_CHAN_NSLOTS    2
#define TEST_CHAN_SEND_ID   4
#define TEST_CHAN_RECV_ID   3
/* We are the sender, and we will be responsible for collecting resulting data */
#ifdef READER_HIGH
#define TEST_CHAN_PRIO_SELF 5
#else
#define TEST_CHAN_PRIO_SELF 4
#endif

typedef unsigned int cycles_32_t;

struct perfdata perf1, perf2, perf3;
cycles_t result1[ITERATION] = {0, };
cycles_t result2[ITERATION] = {0, };
cycles_t result3[ITERATION] = {0, };

int
main(void)
{
	int i;
	cycles_t wakeup;
	cycles_32_t ts1, ts2, ts3;
	int first = 0;
#ifdef USE_EVTMGR
	evt_res_id_t evt_id;
	evt_res_data_t evtdata;
	evt_res_type_t  evtsrc;
#endif

	printc("Component chan sender: executing main.\n");

	/* Send data to receiver so it can register for channels */

#ifdef USE_EVTMGR
	assert(evt_init(&e, 2) == 0);
	evt_id = evt_add(&e, 0, (evt_res_data_t)&r);
	assert(evt_id != 0);
	assert(chan_rcv_evt_associate(&r, evt_id) == 0);
	printc("Sender side event created.\n");
#endif

	/*
	 * This sleep in both hi and lo comps lets the benchmark run
	 * more predictably on HW and on Qemu.
	 *
	 * Likely because this helps the priority change in cos_init take effect!
	 * Or because this lets the initialization of both ends of channels complete before tests start!
	 */
	wakeup = time_now() + time_usec2cyc(100 * 1000);
	sched_thd_block_timeout(0, wakeup);

	for (int i = 0; i < ITERATION + 1; i++) {
		debug("w1,");
		ts1 = time_now();
		debug("ts1: %d,", ts1);
		debug("w2,");
		chan_send(&s, &ts1, 0);
		debug("w3,");
#ifdef USE_EVTMGR
		/* Receive from the events then the channel */
		while (chan_recv(&r, &ts2, CHAN_NONBLOCKING) == CHAN_TRY_AGAIN) evt_get(&e, EVT_WAIT_DEFAULT, &evtsrc, &evtdata);
#else
		chan_recv(&r, &ts2, 0);
#endif
		debug("ts2: %d,", ts2);
		debug("w4,");
		ts3 = time_now();
		debug("w5,");

		if (first == 0) first = 1;
		else {
			if (ts2 > ts1 && ts3 > ts2) {
				perfdata_add(&perf1, ts2 - ts1);
				perfdata_add(&perf2, ts3 - ts2);
				perfdata_add(&perf3, ts3 - ts1);
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

	while(1);
}

void
cos_init(void)
{
	/* Initialize performance data */
	perfdata_init(&perf1, "IPC channel - reader high use this", result1, ITERATION);
	perfdata_init(&perf2, "IPC channel - writer high use this", result2, ITERATION);
	perfdata_init(&perf3, "IPC channel - roundtrip", result3, ITERATION);

	printc("Component chan sender initializing:\n\tJoin channel %d\n", TEST_CHAN_SEND_ID);
	if (chan_snd_init_with(&s, TEST_CHAN_SEND_ID, TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, CHAN_DEFAULT)) {
		printc("Chan test 2 (%ld): Could not initialize send.\n", cos_compid());
		BUG();
	}
	printc("\tJoin channel %d\n", TEST_CHAN_RECV_ID);
	if (chan_rcv_init_with(&r, TEST_CHAN_RECV_ID, TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, CHAN_DEFAULT)) {
		printc("Chan test 2 (%ld): Could not initialize recv.\n", cos_compid());
		BUG();
	}

	printc("Send: smem %p, rmem %p.\n", s.meta.mem, r.meta.mem);

	printc("\tPriority %d for self!\n", TEST_CHAN_PRIO_SELF);
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, TEST_CHAN_PRIO_SELF))) {
		printc("sched_thd_param_set failed.\n");
		BUG();
	}
}
