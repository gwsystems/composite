#include <chan.h>
#include <bitmap.h>
#include <chanmgr.h>

int
chan_snd_init(struct chan_snd *s, struct chan *c)
{
	assert(s && c);
	assert(c->refcnt > 0);

	ps_faa(&c->refcnt, 1);
	*s = (struct chan_snd) {
		.meta = c->meta,
		.c    = c,
	};

	return 0;
}

int
chan_rcv_init(struct chan_rcv *r, struct chan *c)
{
	assert(r && c);
	assert(c->refcnt > 0);

	ps_faa(&c->refcnt, 1);
	*r = (struct chan_rcv) {
		.meta = c->meta,
		.c    = c,
	};

	return 0;
}

static int
__chan_gather_resources(struct __chan_meta *m, chan_id_t id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags)
{
	cbuf_t cb;
	void *mem;
	sched_blkpt_id_t full, empty;
	int ret;

	if ((ret = chanmgr_mem_resources(id, &cb, &mem)))      return ret;
	if ((ret = chanmgr_sync_resources(id, &full, &empty))) return ret;
	assert(mem != NULL && full > 0 && empty > 0);

	*m = (struct __chan_meta) {
		.nslots          = nslots,
		.item_sz         = item_sz,
		.wraparound_mask = (1 << log32(nslots)) - 1,
		.id              = id,
		.cbuf_id         = cb,
		.blkpt_full_id   = full,
		.blkpt_empty_id  = empty,
	};

	__chan_init_with(m, full, empty, mem);

	return 0;
}

int
chan_init(struct chan *c, unsigned int item_sz, unsigned int nslots, chan_flags_t flags)
{
	chan_id_t id;
	int ret;

	assert((flags & CHAN_EXACT_SIZE) == 0);
	assert((flags & CHAN_MPSC) == 0);
	nslots = (unsigned int)nlepow2((u32_t)nslots);

	id = chanmgr_create(item_sz, nslots, flags);
	if (id == 0) return -CHAN_ERR_NOMEM;
	c->refcnt = 1;
	if (__chan_gather_resources(&c->meta, id, item_sz, nslots, flags)) return -CHAN_ERR_INVAL_ARG;

	return 0;
}

int
chan_snd_init_with(struct chan_snd *s, chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags)
{
	s->c = NULL;
	return __chan_gather_resources(&s->meta, cap_id, item_sz, nslots, flags);
}

int
chan_rcv_init_with(struct chan_rcv *r, chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags)
{
	r->c = NULL;
	return __chan_gather_resources(&r->meta, cap_id, item_sz, nslots, flags);
}

static int
chan_release_resources(struct chan *c)
{
	long cnt;

	if (c->refcnt == 0) return 1;

	cnt = ps_faa(&c->refcnt, -1);
	if (cnt > 1) return 1; /* still referenced elsewhere */

	assert(cnt == 0);
	chanmgr_delete(c->meta.id);

	return 0;
}

void
chan_snd_teardown(struct chan_snd *s)
{
	if (!s->c) {
		chanmgr_delete(s->meta.id);
	} else {
		chan_release_resources(s->c);
	}
}

void
chan_rcv_teardown(struct chan_rcv *r)
{
	if (!r->c) {
		chanmgr_delete(r->meta.id);
	} else {
		chan_release_resources(r->c);
	}
}

int
chan_teardown(struct chan *c)
{
	return chan_release_resources(c);
}

struct chan *
chan_snd_get_chan(struct chan_snd *s)
{
	return s->c;
}

struct chan *
chan_rcv_get_chan(struct chan_rcv *r)
{
	return r->c;
}

unsigned int
chan_mem_sz(unsigned int item_sz, unsigned int slots)
{
	return sizeof(struct __chan_mem) + item_sz * slots;
}

int
chan_evt_associate(struct chan *c, evt_res_id_t eid)
{
	int ret;

	if (c->meta.evt_id != 0) return -1;
	c->meta.evt_id = eid;

	ret = chanmgr_evt_set(c->meta.id, c->meta.evt_id, 1);
	/* signal to the communicating pair to update their event resource id. */
	c->meta.mem->producer_update = 1;

	return ret;
}

evt_res_id_t
chan_evt_associated(struct chan *c)
{
	if (c->meta.evt_id == 0) {
		c->meta.evt_id = chanmgr_evt_get(c->meta.id, 1);
	}

	return c->meta.evt_id;
}

int
chan_evt_disassociate(struct chan *c)
{
	evt_res_id_t eid = 0;

	if (c->meta.evt_id == 0) return -1;
	chanmgr_evt_set(c->meta.id, 0, 1);
	c->meta.mem->producer_update = 1;
	c->meta.evt_id = 0;

	return 0;
}
