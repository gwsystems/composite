#include <cos_component.h>
#include <llprint.h>
#include <static_slab.h>
#include <evt.h>
#include <crt_blkpt.h>
#include <ps_refcnt.h>

#define MAX_NUM_EVT 8


struct ring {
	int          producer, consumer;
	evt_res_id_t mem[MAX_NUM_EVT];
};

struct evt_agg {
	compid_t         client;
	struct ring      ring;
	struct ps_refcnt refcnt;
	struct crt_blkpt blkpt;
};

SS_STATIC_SLAB(evt, struct evt_agg, MAX_NUM_THREADS);

struct evt_res {
	evt_res_id_t *  evt_entry; /* -1 = not resident */
	evt_res_id_t    me;
	evt_res_type_t  type;
	evt_res_data_t  data;
	compid_t        client;
	struct evt_agg *evt;
};

SS_STATIC_SLAB(evtres, struct evt_res, MAX_NUM_EVT);

static int
ring_empty(struct ring *r)
{
	return r->producer == r->consumer;
}

static int
ring_full(struct ring *r)
{
	return r->producer + 1 == r->consumer;
}

static int
ring_dequeue(struct ring *r, evt_res_id_t *id)
{
	evt_res_id_t *tmp;

	if (ring_empty(r)) return 1;

	tmp  = &r->mem[r->consumer % MAX_NUM_EVT];
	*id  = *tmp;
	*tmp = 0;
	r->consumer++;

	return 0;
}

static evt_res_id_t *
ring_enqueue(struct ring *r, evt_res_id_t id)
{
	evt_res_id_t *evtres;

	if (ring_full(r)) return NULL;

	evtres  = &r->mem[r->producer % MAX_NUM_EVT];
	*evtres = id;
	r->producer++;

	return evtres;
}

evt_id_t
__evt_alloc(unsigned long max_evts)
{
	struct evt_agg *em = ss_evt_alloc();

	if (!em) return 0;

	memset(em, 0, sizeof(struct evt));
	crt_blkpt_init(&em->blkpt);
	ss_evt_activate(em);

	return ss_evt_id(em);
}

int
__evt_free(evt_id_t id)
{
	struct evt_agg *em = ss_evt_get(id);

	if (!em || ps_refcnt_get(&em->refcnt) != 0) return -1;
	crt_blkpt_teardown(&em->blkpt);
	ss_evt_free(em);

	return 0;
}

int
__evt_get(evt_id_t id, evt_wait_flags_t flags, evt_res_type_t *src, evt_res_data_t *ret_data)
{
	struct crt_blkpt_checkpoint chkpt;
	struct evt_agg *            e = ss_evt_get(id);
	struct evt_res *            res;
	evt_res_id_t                rid;

	if (!e) return -1;

	while (1) {
		crt_blkpt_checkpoint(&e->blkpt, &chkpt);

		if (!ring_dequeue(&e->ring, &rid)) break;
		if (flags & EVT_WAIT_NONBLOCKING) return 1;

		if (crt_blkpt_blocking(&e->blkpt, 0, &chkpt)) continue;
		if (!ring_empty(&e->ring)) continue;
		crt_blkpt_wait(&e->blkpt, 0, &chkpt);
	}

	res = ss_evtres_get(rid);
	assert(res);
	res->evt_entry = NULL;
	*src           = res->type;
	*ret_data      = res->data;

	return 0;
}

evt_res_id_t
__evt_add(evt_id_t id, evt_res_type_t srctype, evt_res_data_t retdata)
{
	struct evt_agg *e = ss_evt_get(id);
	struct evt_res *res;
	evt_res_id_t    rid;

	if (!e) return 0;
	res = ss_evtres_alloc();
	if (!res) return 0;

	rid = ss_evtres_id(res);
	ps_refcnt_take(&e->refcnt);
	*res = (struct evt_res){
	  .client    = cos_inv_token(),
	  .type      = srctype,
	  .data      = retdata,
	  .evt_entry = NULL,
	  .me        = rid,
	  .evt       = e,
	};
	ss_evtres_activate(res);

	return rid;
}

int
__evt_rem(evt_id_t id, evt_res_id_t rid)
{
	struct evt_agg *e = ss_evt_get(id);
	struct evt_res *res;

	if (!e) return -1;
	res = ss_evtres_get(rid);
	if (!res || res->evt != e) return -1;
	ps_refcnt_release(&e->refcnt);

	return 0;
}

int
__evt_trigger(evt_res_id_t rid)
{
	struct evt_agg *e;
	struct evt_res *res;
	evt_res_id_t *  evt_entry;

	res = ss_evtres_get(rid);
	if (!res) return -1;
	e = res->evt;
	assert(e);
	if (res->evt != e) return -1;

	if (res->evt_entry != NULL) return 0; /* already triggered! */
	evt_entry = ring_enqueue(&e->ring, rid);
	assert(evt_entry); /* ring should be large enough for all evts */
	res->evt_entry = evt_entry;
	crt_blkpt_trigger(&e->blkpt, 0);

	return 0;
}
