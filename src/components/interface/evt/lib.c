#include <evt.h>

int
evt_init(struct evt *evt, unsigned long max_evts)
{
	evt_id_t eid = __evt_alloc(max_evts);

	if (eid == 0) return -1;
	evt->id = eid;

	return 0;
}

int
evt_teardown(struct evt *evt)
{
	if (__evt_free(evt->id)) return -1;
	evt->id = 0;

	return 0;
}

int
evt_get(struct evt *evt, evt_wait_flags_t flags, evt_res_type_t *src, evt_res_data_t *ret_data)
{
	return __evt_get(evt->id, flags, src, ret_data);
}

evt_res_id_t
evt_add(struct evt *e, evt_res_type_t srctype, evt_res_data_t ret_data)
{
	return __evt_add(e->id, srctype, ret_data);
}

int
evt_rem(struct evt *e, evt_res_id_t rid)
{
	return __evt_rem(e->id, rid);
}

int
evt_trigger(evt_res_id_t rid)
{
	return __evt_trigger(rid);
}
