/*
 * Copyright 2020, Bite Ye, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_component.h>
#include <llprint.h>
#include <patina.h>
#include <ps.h>
#include <perfdata.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

patina_chan_s_t sid;
patina_chan_r_t rid;
patina_event_t  evt;

#define ITERATION 10 * 1000
#undef READER_HIGH
#define USE_EVTMGR
/* #define PRINT_ALL */

#define TEST_CHAN_ITEM_SZ sizeof(u32_t)
#define TEST_CHAN_NSLOTS 2
#define TEST_CHAN_SEND_ID 4
#define TEST_CHAN_RECV_ID 3
/* We are the sender, and we will be responsible for collecting resulting data */
#ifdef READER_HIGH
#define TEST_CHAN_PRIO_SELF 5
#else
#define TEST_CHAN_PRIO_SELF 4
#endif

typedef unsigned int cycles_32_t;

struct perfdata perf1, perf2, perf3;
cycles_t        result1[ITERATION] = {
  0,
};
cycles_t result2[ITERATION] = {
  0,
};
cycles_t result3[ITERATION] = {
  0,
};

int
main(void)
{
	int         i;
	cycles_t    wakeup;
	cycles_32_t ts1, ts2, ts3;
	int         first = 0;
#ifdef USE_EVTMGR
	evt_res_id_t   evt_id;
	evt_res_data_t evtdata;
	evt_res_type_t evtsrc;
#endif

	printc("Component chan sender: executing main.\n");

	/* Send data to receiver so it can register for channels */

#ifdef USE_EVTMGR
	patina_event_create(&evt, 1);
	patina_event_add(&evt, rid, 0);
	printc("Sender side event created.\n");
#endif

	/*
	 * This sleep in both hi and lo comps lets the benchmark run
	 * more predictably on HW and on Qemu.
	 *
	 * Likely because this helps the priority change in cos_init take effect!
	 * Or because this lets the initialization of both ends of channels complete before tests start!
	 */
	wakeup = time_now() + time_usec2cyc(1000 * 1000);
	sched_thd_block_timeout(0, wakeup);

	for (int i = 0; i < ITERATION + 1; i++) {
		debug("w1,");
		ts1 = time_now();
		debug("ts1: %d,", ts1);
		debug("w2,");
		patina_channel_send(sid, &ts1, 1, 0);
		debug("w3,");
#ifdef USE_EVTMGR
		/* Receive from the events then the channel */
		while (patina_channel_recv(rid, &ts2, 1, CHAN_NONBLOCKING) == CHAN_TRY_AGAIN)
			patina_event_wait(&evt, NULL, 0);
#else
		patina_channel_recv(rid, &ts2, 1, 0);
#endif
		debug("ts2: %d,", ts2);
		debug("w4,");
		ts3 = time_now();
		debug("w5,");

		if (first == 0)
			first = 1;
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

	while (1)
		;
}

void
cos_init(void)
{
	perfdata_init(&perf1, "IPC channel - reader high use this", result1, ITERATION);
	perfdata_init(&perf2, "IPC channel - writer high use this", result2, ITERATION);
	perfdata_init(&perf3, "IPC channel - roundtrip", result3, ITERATION);

	printc("Component chan sender initializing:\n\tJoin channel %d\n", TEST_CHAN_SEND_ID);
	sid = patina_channel_retrieve_send(TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, TEST_CHAN_SEND_ID);

	printc("\tJoin channel %d\n", TEST_CHAN_RECV_ID);
	rid = patina_channel_retrieve_recv(TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, TEST_CHAN_RECV_ID);

	printc("\tPriority %d for self!\n", TEST_CHAN_PRIO_SELF);
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, TEST_CHAN_PRIO_SELF))) {
		printc("sched_thd_param_set failed.\n");
		BUG();
	}
}
