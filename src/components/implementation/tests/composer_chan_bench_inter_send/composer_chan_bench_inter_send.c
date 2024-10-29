/*
 * Copyright 2020, Bite Ye, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_component.h>
#include <llprint.h>
#include <cos_time.h>
#include <patina.h>
#include <ps.h>
#include <perfdata.h>
#include <initargs.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

patina_chan_s_t sid;
patina_chan_r_t rid;
patina_event_t  evt;

struct chan_snd s;
struct chan_rcv r;

#define ITERATION 10 * 1000
#undef USE_EVTMGR
/* #define PRINT_ALL */

#define TEST_CHAN_ITEM_SZ sizeof(u32_t)
#define TEST_CHAN_NSLOTS 2
#define TEST_CHAN_SEND_ID 4
#define TEST_CHAN_RECV_ID 3
/* We are the sender, and we will be responsible for collecting resulting data */
#define TEST_CHAN_PRIO_SELF 5

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
// 	int         i;
// 	cycles_t    wakeup;
// 	cycles_32_t ts1, ts2, ts3;
// #ifdef USE_EVTMGR
// 	evt_res_id_t   evt_id;
// 	evt_res_data_t evtdata;
// 	evt_res_type_t evtsrc;
// #endif

// 	printc("Component chan sender: executing main.\n");

// 	/* Send data to receiver so it can register for channels */

// #ifdef USE_EVTMGR
// 	patina_event_create(&evt, 1);
// 	patina_event_add(&evt, rid, 0);
// 	printc("Sender side event created.\n");
// #endif

// 	/*
// 	 * This sleep in both hi and lo comps lets the benchmark run
// 	 * more predictably on HW and on Qemu.
// 	 *
// 	 * Likely because this helps the priority change in cos_init take effect!
// 	 * Or because this lets the initialization of both ends of channels complete before tests start!
// 	 */
// 	wakeup = time_now() + time_usec2cyc(1000 * 1000);
// 	sched_thd_block_timeout(0, wakeup);

// 	for (int i = 0; i < ITERATION; i++) {
// 		debug("w1,");
// 		ts1 = time_now();
// 		debug("ts1: %d,", ts1);
// 		debug("w2,");
// 		patina_channel_send(sid, &ts1, 1, 0);
// 		debug("w3,");
// #ifdef USE_EVTMGR
// 		/* Receive from the events then the channel */
// 		while (patina_channel_recv(rid, &ts2, 1, CHAN_NONBLOCKING) == CHAN_TRY_AGAIN)
// 			patina_event_wait(&evt, NULL, 0);
// #else
// 		patina_channel_recv(rid, &ts2, 1, 0);
// #endif
// 		debug("ts2: %d,", ts2);
// 		debug("w4,");
// 		ts3 = time_now();
// 		debug("w5,");

// 		if (ts2 > ts1 && ts3 > ts2) {
// 			perfdata_add(&perf1, ts2 - ts1);
// 			perfdata_add(&perf2, ts3 - ts2);
// 			perfdata_add(&perf3, ts3 - ts1);
// 		}
// 	}

// #ifdef PRINT_ALL
// 	perfdata_raw(&perf1);
// 	perfdata_raw(&perf2);
// 	perfdata_raw(&perf3);
// #endif
// 	perfdata_calc(&perf1);
// 	perfdata_calc(&perf2);
// 	perfdata_calc(&perf3);

// 	perfdata_print(&perf1);
// 	perfdata_print(&perf2);
// 	perfdata_print(&perf3);

// 	while (1);
}

void
cos_init(void)
{
	perfdata_init(&perf1, "IPC channel - reader high use this", result1, ITERATION);
	perfdata_init(&perf2, "IPC channel - writer high use this", result2, ITERATION);
	perfdata_init(&perf3, "IPC channel - roundtrip", result3, ITERATION);

	memset(&s, 0, sizeof(struct chan_snd));

	struct initargs param_entries, chan_curr, chan_entries;
	struct initargs_iter j;
	int ret, cont;

	ret = args_get_entry("comp_virt_resources/chan", &chan_entries);
	assert(!ret);

	for (cont = args_iter(&chan_entries, &j, &chan_curr); cont; cont = args_iter_next(&j, &chan_curr)) {
		char *name_str 		= args_get_from("name", &chan_curr);
		char *id_str 		= NULL;
		char *size_item_str = NULL;
		char *num_slots_str = NULL;
		char *vr_type 		= NULL;
		char *evt_id_str 	= NULL;
		chan_id_t chan_id 	= 0xFF;
		struct initargs assoc_entries, assoc_curr;
		struct initargs_iter i;
		int cont_i;

		id_str = args_get_from("id", &chan_curr);

		ret = args_get_entry_from("param", &chan_curr, &param_entries);
		assert(!ret);

		size_item_str = args_get_from("size_item", &param_entries);
		num_slots_str = args_get_from("num_slots", &param_entries);

		printc("MBAI-TEST; init chan id is %s  \n", id_str);
		printc("MBAI-TEST; init size_item  is %s  \n", size_item_str);
		printc("MBAI-TEST; init num_slots  is %s  \n", num_slots_str);

		ret = args_get_entry_from("association",&chan_curr, &assoc_entries);
		if ( ret == 0 ) {
			//TODO: test works without associate the evt_id, might be a bug, needs to check this. 
			for (cont_i = args_iter(&assoc_entries, &i, &assoc_curr); cont_i; cont_i = args_iter_next(&i, &assoc_curr)) {
				vr_type = args_get_from("vr_type", &assoc_curr);
				printc("MBAI-TEST; init vr_type is %s  \n", vr_type);
				if(vr_type && strcmp(vr_type, "evt") == 0){
					evt_id_str = args_get_from("inst_id", &assoc_curr);
					printc("MBAI-TEST; init inst_id is %s  \n", evt_id_str);
				}
			}
		}

		if (name_str && strcmp(name_str, "chan_in") == 0) {
			ret = chan_snd_init_with(&s, atoi(id_str), atoi(size_item_str), atoi(num_slots_str), CHAN_DEFAULT);
			assert(!ret);
			if ( evt_id_str != NULL) chan_snd_evt_associate(&s,atoi(evt_id_str));
		}

		if (name_str && strcmp(name_str, "chan_out") == 0) {
			ret = chan_rcv_init_with(&r, atoi(id_str), atoi(size_item_str), atoi(num_slots_str), CHAN_DEFAULT);
			assert(!ret);
			if ( evt_id_str != NULL) chan_rcv_evt_associate(&r,atoi(evt_id_str));
		}
	}	
	
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, TEST_CHAN_PRIO_SELF))) {
		printc("sched_thd_param_set failed.\n");
		BUG();
	}
}
