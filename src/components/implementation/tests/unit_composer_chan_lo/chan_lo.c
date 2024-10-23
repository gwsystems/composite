#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <initargs.h>
#include <ps.h>
#include <cos_time.h>

struct chan_snd s;
struct chan_rcv r;

int
main(void)
{
	cycles_t wakeup;
	u64_t i8test_data = 'A';
	printc("Component chan low: executing main.\n");

	wakeup = time_now() + time_usec2cyc(10 * 1000);
	sched_thd_block_timeout(0, wakeup);

	printc("chan_lo start sending \n");

	ps_tsc_t tsc = ps_tsc();
	if (chan_send(&s, &tsc, 0)) {
		printc("chan_snd error\n");
		assert(0);
	}
}

/*int
cos_init(void)
{
	printc("Component chan low initilaization.\n");
	memset(&s, 0, sizeof(struct chan_snd));

    struct initargs chan_entries, param_entries, assoc_entries;
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
	chan_snd_init_with(&s, atoi(id_str), atoi(size_item_str), atoi(num_slots_str), CHAN_DEFAULT);

	ret = args_get_entry("comp_virt_resources/chan/chan_01/association/_", &assoc_entries);
    assert(!ret);

	char *evt_id_str = args_get_from("inst_id", &assoc_entries);

	chan_snd_evt_associate(&s,atoi(evt_id_str));
	return 0;
}*/

int
cos_init(void)
{
	printc("Component chan low initilaization.\n");
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
	
	return 0;
}
