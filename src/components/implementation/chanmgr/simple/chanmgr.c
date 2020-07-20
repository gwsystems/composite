#include <cos_component.h>
#include <llprint.h>

#include <chanmgr.h>
#include <chan.h>
#include <static_alloc.h>

#define MAX_NUM_CHAN 16
struct chan_info {
	struct __chan_meta info;
	compid_t snd, rcv;
	vaddr_t snd_mem, rcv_mem;
	unsigned int npages;
	cbuf_t buf_id;
};

SA_STATIC_ALLOC_OFF(channel, struct chan_info, MAX_NUM_CHAN, 1);

chan_id_t
chanmgr_create(unsigned int item_sz, unsigned int slots, chan_flags_t flags)
{
	chan_id_t id, ret;
	struct chan_info *c;
	struct crt_blkpt empty, full;
	unsigned int mem_pages;

	c = sa_channel_alloc();
	if (!c) return 0;
	ret = id = sa_channel_id(c);

	c->info = (struct __chan_meta) {
		.nslots  = slots,
		.item_sz = item_sz,
		.flags   = flags,
		.id      = id
	};
	if (crt_blkpt_init(&empty)) ERR_THROW(0, free_chan);
	if (crt_blkpt_init(&full))  ERR_THROW(0, dealloc_empty_blkpt);
	c->info.blkpt_empty_id = empty.id;
	c->info.blkpt_full_id  = full.id;
	mem_pages = round_up_to_page(chan_mem_sz(item_sz, slots)) / PAGE_SIZE;
	c->buf_id = memmgr_shared_page_allocn(mem_pages, (vaddr_t *)&c->info.mem);
	if (c->buf_id == 0) ERR_THROW(0, dealloc_full_blkpt);

	sa_channel_activate(c);

	return ret;

dealloc_full_blkpt:
	crt_blkpt_teardown(&full);
dealloc_empty_blkpt:
	crt_blkpt_teardown(&empty);
free_chan:
	sa_channel_free(c);

	return ret;
}

int
chanmgr_sync_resources(chan_id_t id, sched_blkpt_id_t *full, sched_blkpt_id_t *empty)
{
	struct chan_info *chinfo;
	compid_t cid = cos_inv_token();

	*full = *empty = 0;
	chinfo = sa_channel_get(id);
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
	*mem   = NULL;
	chinfo = sa_channel_get(id);
	if (!chinfo) return -1;

	*cb_id = chinfo->buf_id;
	*mem   = chinfo->info.mem;

	return 0;
}

int
chanmgr_delete(chan_id_t id)
{
	/* TODO */
	return 0;
}

void
cos_init(void)
{

}
