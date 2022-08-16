#include <cos_component.h>
#include <llprint.h>

#include <chanmgr.h>
#include <chan.h>
#include <static_slab.h>

#define MAX_NUM_CHAN 32
struct chan_info {
	struct __chan_meta info;
	unsigned int npages;
	cbuf_t buf_id;
	vaddr_t mem;
	evt_res_id_t evt_id;
};

SS_STATIC_SLAB(channel, struct chan_info, MAX_NUM_CHAN);

static chan_id_t
__chanmgr_create(unsigned int item_sz, unsigned int slots, chan_flags_t flags, chan_id_t chanid)
{
	chan_id_t id, ret;
	struct chan_info *c;
	struct sync_blkpt empty, full;
	unsigned int mem_pages;

	if (chanid == 0) {
		c = ss_channel_alloc();
	} else {
		c = ss_channel_alloc_at_id(chanid);
	}
	if (!c) return 0;
	ret = id = ss_channel_id(c);

	c->info = (struct __chan_meta) {
		.nslots  = slots,
		.item_sz = item_sz,
		.flags   = flags,
		.id      = id,
		.evt_id  = 0
	};
	if (sync_blkpt_init(&empty)) ERR_THROW(0, free_chan);
	if (sync_blkpt_init(&full))  ERR_THROW(0, dealloc_empty_blkpt);
	c->info.blkpt_empty_id = empty.id;
	c->info.blkpt_full_id  = full.id;
	mem_pages = round_up_to_page(chan_mem_sz(item_sz, slots)) / PAGE_SIZE;
	c->buf_id = memmgr_shared_page_allocn(mem_pages, (vaddr_t *)&c->info.mem);
	if (c->buf_id == 0) ERR_THROW(0, dealloc_full_blkpt);

	ss_channel_activate(c);

	return ret;

dealloc_full_blkpt:
	sync_blkpt_teardown(&full);
dealloc_empty_blkpt:
	sync_blkpt_teardown(&empty);
free_chan:
	ss_channel_free(c);

	return ret;
}

chan_id_t
chanmgr_create(unsigned int item_sz, unsigned int slots, chan_flags_t flags)
{
	return __chanmgr_create(item_sz, slots, flags, 0);
}

int
chanmgr_sync_resources(chan_id_t id, sched_blkpt_id_t *full, sched_blkpt_id_t *empty)
{
	struct chan_info *chinfo;
	compid_t cid = cos_inv_token();

	*full = *empty = 0;
	chinfo = ss_channel_get(id);
	if (!chinfo) return -1;

	*full  = chinfo->info.blkpt_full_id;
	*empty = chinfo->info.blkpt_empty_id;

	return 0;
}

int
chanmgr_mem_resources(chan_id_t id, cbuf_t *cb_id, void **mem)
{
	struct chan_info *chinfo;
	compid_t cid = cos_inv_token();

	*cb_id = 0;
	if (mem) *mem = NULL;
	chinfo = ss_channel_get(id);
	if (!chinfo) return -1;

	*mem   = (void *)chinfo->mem;
	*cb_id = chinfo->buf_id;

	return 0;
}

int
chanmgr_evt_set(chan_id_t id, evt_res_id_t rid, int sender_to_reciever)
{
	struct chan_info *ch;

	/* Only allow event aggregation for receivers */
	if (!sender_to_reciever) return -1;

	ch = ss_channel_get(id);
	if (!ch) return -1;
	if (ch->evt_id && rid != 0) return -1;

	ch->evt_id = rid;

	return 0;
}

evt_res_id_t
chanmgr_evt_get(chan_id_t id, int sender_to_reciever)
{
	struct chan_info *ch;

	/* Only allow triggering for senders */
	if (!sender_to_reciever) return -1;

	ch = ss_channel_get(id);
	if (!ch) return -1;

	return ch->evt_id;
}

int
chanmgr_delete(chan_id_t id)
{
	/* TODO */
	return -1;
}

struct init_info {
	chan_id_t id;
	unsigned int nitems, itemsz;
};

#define CHAN_INIT(_id, _n, _sz)				\
	(struct init_info) {				\
		.id     = _id,				\
		.nitems = _n,			        \
	        .itemsz = _sz				\
	}

/* These are the initial channels - do not modify parameters! */
struct init_info init_chan[] = {
	CHAN_INIT(1, 128, sizeof(u64_t)),
	CHAN_INIT(2, 128, sizeof(u64_t)),
	CHAN_INIT(3, 2, sizeof(u32_t)),
	CHAN_INIT(4, 2, sizeof(u32_t)),
	CHAN_INIT(0, 0, 0),
};

void
cos_init(void)
{
	int i;

	printc("Chanmgr (%ld): creating static, initial channels.\n", cos_compid());

	for (i = 0; init_chan[i].id > 0; i++) {
		struct init_info *ch = &init_chan[i];
		chan_id_t id;

		id = __chanmgr_create(ch->itemsz, ch->nitems, 0, ch->id);
		if (id != ch->id) BUG();
	}
}
