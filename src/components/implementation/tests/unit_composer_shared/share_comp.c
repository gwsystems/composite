#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <initargs.h>
#include <ps.h>
#include <cos_time.h>

struct chan_rcv r;
struct chan_snd s;
struct evt e;

int
main(void)
{
	char i8test_data = 0;
	ps_tsc_t snd;
	evt_res_data_t evtdata;
	evt_res_type_t  evtsrc;
	cycles_t wakeup;

	printc("Component chan shared comp: executing main.\n");
	wakeup = time_now() + time_usec2cyc(10 * 1000);
	sched_thd_block_timeout(0, wakeup);


	/* Receive from the events */
	if (evt_get(&e, EVT_WAIT_DEFAULT, &evtsrc, &evtdata))  {
		printc("evt_get error\n");
		assert(0);
	}

	if (chan_recv(&r, &snd, 0)) {
		printc("chan_recv error\n");
		assert(0);
	}

/*	if (i8test_data == 'A') {
		printc("SUCCESS: unit_composer_share test\n");
	}
	else {
		printc("FAIL: unit_composer_share test \n");
	}*/

	if(chan_send(&s, &snd,0)){
		printc("chan_snd error\n");
		assert(0);
	}
}

/*int
cos_init(void)
{
	printc("Component shared comp executing main.\n");

    struct initargs chan_entries, evt_entries, param_entries, assoc_entries;
	struct initargs_iter i;
	int ret, cont;
	int chan_id;
	ret = args_get_entry("comp_virt_resources/chan/chan_01", &chan_entries);
    assert(!ret);

	char *id_str 		= NULL;
	char *size_item_str = NULL;
	char *num_slots_str = NULL;

	id_str 	= args_get_from("id", &chan_entries);
	ret 	= args_get_entry("comp_virt_resources/chan/chan_01/param", &param_entries);
	assert(!ret);
	size_item_str = args_get_from("size_item", &param_entries);
	num_slots_str = args_get_from("num_slots", &param_entries);
	printc("MBAI-TEST; init chan id is %s  \n"		, id_str);
	printc("MBAI-TEST; init size_item  is %s  \n"	, size_item_str);
	printc("MBAI-TEST; init num_slots  is %s  \n"	, num_slots_str);
	chan_rcv_init_with(&r, atoi(id_str), atoi(size_item_str), atoi(num_slots_str), CHAN_DEFAULT);

	ret = args_get_entry("comp_virt_resources/evt/evt_01", &evt_entries);
    assert(!ret);

	char *evt_agg_id_str = args_get_from("id", &evt_entries);
	e.id = atoi(evt_agg_id_str);
	ret = args_get_entry("comp_virt_resources/evt/evt_01/association/_", &assoc_entries);
    assert(!ret);

	char *evt_id_str = args_get_from("inst_id", &assoc_entries);
	printc("MBAI-TEST; evt_agg_id_str  is %s  \n"	, evt_agg_id_str);
	printc("MBAI-TEST; evt_id_str  is %s  \n"	, evt_id_str);
	ret = evt_add_at_id(atoi(evt_agg_id_str), atoi(evt_id_str), 1, (evt_res_data_t)&r);
	assert(ret != 0);
	chan_rcv_evt_associate(&r, atoi(evt_id_str));


	ret = args_get_entry("comp_virt_resources/chan/chan_02", &chan_entries);
    assert(!ret);

	id_str 	= args_get_from("id", &chan_entries);
	ret 	= args_get_entry("comp_virt_resources/chan/chan_02/param", &param_entries);
	assert(!ret);
	size_item_str = args_get_from("size_item", &param_entries);
	num_slots_str = args_get_from("num_slots", &param_entries);
	printc("MBAI-TEST; init chan id is %s  \n"		, id_str);
	printc("MBAI-TEST; init size_item  is %s  \n"	, size_item_str);
	printc("MBAI-TEST; init num_slots  is %s  \n"	, num_slots_str);
	chan_snd_init_with(&s, atoi(id_str), atoi(size_item_str), atoi(num_slots_str), CHAN_DEFAULT);
	return 0;
}*/

int
cos_init(void)
{
	printc("Component shared comp executing main.\n");

    struct initargs chan_entries, chan_curr, evt_entries, evt_curr, param_entries, assoc_entries, assoc_curr;
	struct initargs_iter i,j;
	int ret, cont, cont_j;
	int chan_id;

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
	for (cont = args_iter(&chan_entries, &i, &chan_curr); cont; cont = args_iter_next(&i, &chan_curr)) {
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
			for (cont_j = args_iter(&assoc_entries, &j, &assoc_curr); cont_j; cont_j = args_iter_next(&j, &assoc_curr)) {
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
		char *vr_type = NULL;
		char *evt_agg_id_str = NULL;
		char *evt_agg_data_str = NULL;
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

		if ( strcmp(evt_agg_data_str, "chan") == 0) {
			res_data = (evt_res_data_t)&r;
		} else {
			assert(0 && "unknown resource associated data");
		}
	
		printc("MBAI-TEST; init vr_type is %s  \n", vr_type);
		printc("MBAI-TEST; evt_agg_id_str is %s \n", evt_agg_id_str);
		printc("MBAI-TEST; evt_agg_data_str is %s \n", evt_agg_data_str);
		printc("MBAI-TEST; evt_id_str is %s \n", evt_id_str);

		if ( evt_agg_id_str != NULL ) {
			ret = evt_add_at_id(atoi(evt_agg_id_str), atoi(evt_id_str), 1, res_data);
			assert(ret != 0);
			e.id = atoi(evt_agg_id_str);
		}    
    }

    return 0;
}
