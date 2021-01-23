#ifndef EVT_PRIVATE_H
#define EVT_PRIVATE_H

typedef word_t evt_id_t;

/*
 * This is a struct because we want to be able to extend it to contain
 * amortization features (e.g. a channel) to receive multiple
 * events.
 */
struct evt {
	evt_id_t id;
};

evt_id_t     __evt_alloc(unsigned long max_evts);
int          __evt_free(evt_id_t id);
int          __evt_get(evt_id_t id, evt_wait_flags_t flags, evt_res_type_t *src, evt_res_data_t *ret_data);
evt_res_id_t __evt_add(evt_id_t id, evt_res_type_t srctype, evt_res_data_t ret_data);
int          __evt_rem(evt_id_t id, evt_res_id_t rid);
int          __evt_trigger(evt_res_id_t rid);

#endif /* EVT_PRIVATE_H */
