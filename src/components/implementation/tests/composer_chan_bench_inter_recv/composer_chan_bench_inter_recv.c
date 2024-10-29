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
#include <evt.h>
#include <tmr.h>
#include <chan.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

patina_chan_s_t sid;
patina_chan_r_t rid;
patina_event_t  evt;

struct chan_rcv r;
struct chan_snd s;
struct tmr t;
struct evt e; /* each event aggreagate should have one evt e */


/* Keep these settings below consistent with the sender side */
#undef USE_EVTMGR

#define TEST_CHAN_ITEM_SZ sizeof(u32_t)
#define TEST_CHAN_NSLOTS 2
#define TEST_CHAN_SEND_ID 3
#define TEST_CHAN_RECV_ID 4
#define TEST_CHAN_PRIO_SELF 4

typedef unsigned int cycles_32_t;

int
main(void)
{
// 	cycles_t    wakeup;
// 	cycles_32_t tmp;
// #ifdef USE_EVTMGR
// 	evt_res_id_t   evt_id;
// 	evt_res_data_t evtdata;
// 	evt_res_type_t evtsrc;
// #endif

// 	printc("Component chan receiver: executing main.\n");

// 	/* See if event manager is in use. If yes, log the receiver channel into it */
// #ifdef USE_EVTMGR
// 	patina_event_create(&evt, 1);
// 	patina_event_add(&evt, rid, 0);
// 	printc("Receiver side event created.\n");
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

// 	/* Never stops running; sender controls how many iters to run. */
// 	while (1) {
// 		debug("r1,");
// #ifdef USE_EVTMGR
// 		/* Receive from the events then the channel */
// 		while (patina_channel_recv(rid, &tmp, 1, CHAN_NONBLOCKING) == CHAN_TRY_AGAIN)
// 			patina_event_wait(&evt, NULL, 0);
// #else
// 		patina_channel_recv(rid, &tmp, 1, 0);
// #endif
// 		debug("tsr1: %d,", tmp);
// 		debug("r2,");
// 		tmp = time_now();
// 		debug("tsr2: %d,", tmp);
// 		debug("r3,");
// 		patina_channel_send(sid, &tmp, 1, 0);
// 		debug("r4,");
// 	}
}


/* We initialize channel and threads before executing main - here the scheduler doesn't even work so we guarantee good
 * init */
void
cos_init(void)
{

	struct initargs chan_entries, chan_curr, evt_entries, evt_curr, tmr_curr, tmr_entries, param_entries, assoc_entries, assoc_curr;
	struct initargs_iter j,i;
	int ret, cont, cont_i;
	int chan_id;

	ret = args_get_entry("comp_virt_resources/tmr", &tmr_entries);
	assert(!ret);

	for (cont = args_iter(&tmr_entries, &j, &tmr_curr); cont; cont = args_iter_next(&j, &tmr_curr)) {
		char *name_str 		= args_get_from("name", &tmr_curr);
		char *id_str 		= NULL;
		char *time_str 		= NULL;
		char *type_str 		= NULL;
		char *vr_type 		= NULL;
		char *evt_id_str 	= NULL;
		tmr_id_t tmr_id 	= 0xFF;

		id_str = args_get_from("id", &tmr_curr);

		ret = args_get_entry_from("param", &tmr_curr, &param_entries);
		assert(!ret);

		time_str = args_get_from("time", &param_entries);
		type_str = args_get_from("type", &param_entries);

		printc("MBAI-TEST; init tmr id is %s  \n", id_str);
		printc("MBAI-TEST; init time_str  is %s  \n", time_str);
		printc("MBAI-TEST; init type_str  is %s  \n", type_str);

		ret = args_get_entry_from("association",&tmr_curr, &assoc_entries);
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

		ret = tmr_init_static(&t, atoi(time_str), atoi(type_str), atoi(id_str));
		assert(!ret);
		if ( evt_id_str != NULL) {
			ret = tmr_evt_associate(&t, atoi(evt_id_str)); 
			assert(!ret);
		}
	}

	ret = args_get_entry("comp_virt_resources/chan", &chan_entries);
	assert(!ret);
	/*iterate through the structure below:
		- chan
			-	name
			-	id
			-	param
					-	size_item
					-	num_slots
			-	association
					-	evt
	*/
	for (cont = args_iter(&chan_entries, &j, &chan_curr); cont; cont = args_iter_next(&j, &chan_curr)) {
		char *name_str = args_get_from("name", &chan_curr);
		char *id_str = NULL;
		char *size_item_str = NULL;
		char *num_slots_str = NULL;
		char *vr_type = NULL;
		char *evt_id_str = NULL;

		/* Get the resource id, item size, number of slots */
		id_str = args_get_from("id", &chan_curr);
		ret = args_get_entry_from("param", &chan_curr, &param_entries);
		assert(!ret);
		size_item_str = args_get_from("size_item", &param_entries);
		num_slots_str = args_get_from("num_slots", &param_entries);

		printc("MBAI-TEST; init chan name is %s \n", name_str);
		printc("MBAI-TEST; init chan id is %s \n", id_str);
		printc("MBAI-TEST; init size_item is %s \n", size_item_str);
		printc("MBAI-TEST; init num_slots is %s \n", num_slots_str);

		ret = args_get_entry_from("association",&chan_curr, &assoc_entries);
		if (ret == 0)
		{
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
			if (evt_id_str != NULL) chan_rcv_evt_associate(&r,atoi(evt_id_str));
		}
	}

    ret = args_get_entry("comp_virt_resources/evt", &evt_entries);
    assert(!ret);
	/*iterate through the structure below:
		- evt
			-	name
			-	id
			-	association
					-	evt_aggregate	
	*/
    for (cont = args_iter(&evt_entries, &j, &evt_curr); cont; cont = args_iter_next(&j, &evt_curr)) {
        char *name_str = args_get_from("name", &evt_curr);
		struct initargs assoc_entries, assoc_curr;
		struct initargs_iter i;
		int cont_i;
		char *vr_type 			= NULL;
		char *evt_agg_id_str 	= NULL;
		char *evt_agg_data_str	= NULL;
		evt_res_data_t res_data;

		char *evt_id_str = args_get_from("id", &evt_curr);
		ret = args_get_entry_from("association",&evt_curr, &assoc_entries);
		if (ret == 0) {
			for (cont_i = args_iter(&assoc_entries, &i, &assoc_curr); cont_i; cont_i = args_iter_next(&i, &assoc_curr)) {
				vr_type = args_get_from("vr_type", &assoc_curr);
				if(vr_type && strcmp(vr_type, "evt_aggregate") == 0){
					evt_agg_id_str 		= args_get_from("inst_id", &assoc_curr);
					evt_agg_data_str	= args_get_from("data", &assoc_curr);
				}
			}
		}

		printc("MBAI-TEST; init vr_type is %s  \n", vr_type);
		printc("MBAI-TEST; evt_agg_id_str is %s \n", evt_agg_id_str);
		printc("MBAI-TEST; evt_agg_data_str is %s \n", evt_agg_data_str);
		printc("MBAI-TEST; evt_id_str is %s \n", evt_id_str);

		if ( strcmp(evt_agg_data_str, "chan") == 0 ) {
			res_data = (evt_res_data_t)&r;
		} else if ( strcmp(evt_agg_data_str, "tmr") == 0 ) {
			res_data = (evt_res_data_t)&t;
		} else {
			assert(0 && "unknown resource associated data");
		}

		if ( evt_agg_id_str != NULL ) {
			ret = evt_add_at_id(atoi(evt_agg_id_str), atoi(evt_id_str), 1, res_data);
			assert(ret != 0);
			e.id = atoi(evt_agg_id_str);
		}    
    }

	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, TEST_CHAN_PRIO_SELF))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}
}
