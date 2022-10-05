#ifndef CHAN_PRIVATE_H
#define CHAN_PRIVATE_H

#include <cos_component.h>
#include <sync_blkpt.h>
#include <chanmgr_evt.h>
#include <chan_types.h>

/*
 * Channel metadata associated with the chan, and copied into chan_snd
 * and chan_rcv
 */
struct __chan_meta {
	struct __chan_mem *mem;
	sched_blkpt_id_t blkpt_empty_id, blkpt_full_id;
	u32_t item_sz, nslots, wraparound_mask;
	evt_res_id_t evt_id;
	chan_flags_t flags;
	cbuf_t cbuf_id;
	chan_id_t id;
};

/*
 * Padding is to avoid cache-line false sharing, which doesn't exist
 * on a single core. Padding to CACHE_LINE * 2 as Intel units of
 * coherency seem to be 128 bytes rather than 64 (CACHE_LINE).
 */
#if NUM_CPU > 1
#define CHAN_PADDING(num, prevsz) char _padding##num[CACHE_LINE * 2 - prevsz];
#else
#define CHAN_PADDING(num, prevsz)
#endif

/*
 * This is the shared channel data-structure, often in shared
 * memory. Note that consequently we should interpret most of this
 * data as potentially faulty. Thus the blockpoint identifiers are
 * stored redundantly in non-shared memory. If synchronous (blocking)
 * APIs are used, then it is possible to have unbounded blocking.
 */
struct __chan_mem {
	u32_t producer;
	u32_t producer_update;
	/* If the ring is empty, recving threads will block on this blkpt. */
	struct sync_blkpt empty;
	CHAN_PADDING(1, (sizeof(struct sync_blkpt) + 2 * sizeof(u32_t)));
 	u32_t consumer;
	u32_t consumer_update;
	/* If the ring is full, sending thread will block on this blkpt. */
	struct sync_blkpt full;
	CHAN_PADDING(2, (sizeof(struct sync_blkpt) + 2* sizeof(u32_t)));
	/* The memory for the channel. */
	char mem[0];
};

struct chan {
	struct __chan_meta meta;
	unsigned long refcnt;
};

struct chan_snd {
	struct __chan_meta meta;
	struct chan *c;
};

struct chan_rcv {
	struct __chan_meta meta;
	struct chan *c;
};

#include <chanmgr.h>

static inline void
__chan_init_with(struct __chan_meta *meta, sched_blkpt_id_t full, sched_blkpt_id_t empty, void *mem)
{
	struct __chan_mem *m = mem;

	/* Certainly don't "initialize" if channel has been produced into! */
	if (m->producer != 0) return;

	sync_blkpt_init_w_id(&m->empty, empty);
	sync_blkpt_init_w_id(&m->full,  full);

	m->producer = m->consumer = 0;

	meta->mem = m;

	return;
}

static inline unsigned int
__chan_buff_idx_pow2(u32_t v, u32_t wraparound_mask)
{ return v & wraparound_mask; }

static inline int
__chan_full_pow2(struct __chan_mem *m, u32_t wraparound_mask)
{ return __chan_buff_idx_pow2(m->consumer, wraparound_mask) == __chan_buff_idx_pow2(m->producer + 1, wraparound_mask); }

static inline int
__chan_empty_pow2(struct __chan_mem *m, u32_t wraparound_mask)
{ return m->producer == m->consumer; }

static inline int
__chan_produce_pow2(struct __chan_mem *m, void *d, u32_t wraparound_mask, u32_t item_sz)
{
	if (__chan_full_pow2(m, wraparound_mask)) return 1;
	memcpy(m->mem + (__chan_buff_idx_pow2(m->producer, wraparound_mask) * item_sz), d, item_sz);
	m->producer++;

	return 0;
}

static inline int
__chan_consume_pow2(struct __chan_mem *m, void *d, u32_t wraparound_mask, u32_t item_sz)
{
	void *ret;

	if (__chan_empty_pow2(m, wraparound_mask)) return 1;
	memcpy(d, m->mem + (__chan_buff_idx_pow2(m->consumer, wraparound_mask) * item_sz), item_sz);
	m->consumer++;

	return 0;
}

void __chan_meta_evt_update(struct __chan_meta *meta);

/**
 * The next two functions pass all of the variables in via arguments,
 * so that we can use them for constant propagation along with
 * inlining to get rid of the general memcpy code.
 *
 * - @return -
 *
 *     - `-n` on error,
 *     - `>0` on failure to send/recv (non-blocking), and
 *     - `0` on "send/recv complete".
 */
static inline int
__chan_send_pow2(struct chan_snd *s, void *item, u32_t wraparound_mask, u32_t item_sz, int blking)
{
	struct __chan_mem *m = s->meta.mem;

	while (1) {
		struct sync_blkpt_checkpoint chkpt;

		sync_blkpt_checkpoint(&m->full, &chkpt);
		if (!__chan_produce_pow2(m, item, wraparound_mask, item_sz)) {
			struct __chan_meta *meta = &s->meta;

			/* success! */
			sync_blkpt_id_trigger(&m->empty, s->meta.blkpt_empty_id, 0);
			if (unlikely(meta->mem->producer_update)) {
				meta->mem->producer_update = 0;
				__chan_meta_evt_update(meta);
			}
			if (meta->evt_id) {
				if (evt_trigger(meta->evt_id)) return -1;
			}
			break;
		}
		if (!blking) return 1;

		/* Post that we want to block */
		if (sync_blkpt_id_blocking(&m->full, s->meta.blkpt_full_id, 0, &chkpt)) continue;
		/* has a preemption before wait opened an empty slot? */
		if (!__chan_full_pow2(m, wraparound_mask)) continue;
		sync_blkpt_id_wait(&m->full, s->meta.blkpt_full_id, 0, &chkpt);
	}

	return 0;
}

static inline int
__chan_recv_pow2(struct chan_rcv *r, void *item, u32_t wraparound_mask, u32_t item_sz, int blking)
{
	struct __chan_mem *m = r->meta.mem;

	while (1) {
		struct sync_blkpt_checkpoint chkpt;

		sync_blkpt_checkpoint(&m->empty, &chkpt);
		if (!__chan_consume_pow2(m, item, wraparound_mask, item_sz)) {
			/* success! */
			sync_blkpt_id_trigger(&m->full, r->meta.blkpt_full_id, 0);
			break;
		}
		if (!blking) return 1;
		/* Post that we want to block */
		if (sync_blkpt_id_blocking(&m->empty, r->meta.blkpt_empty_id, 0, &chkpt)) continue;
		/* has a preemption before wait added data into a slot? */
		if (!__chan_empty_pow2(m, wraparound_mask)) continue;
		sync_blkpt_id_wait(&m->empty, r->meta.blkpt_empty_id, 0, &chkpt);
	}

	return 0;
}

/* How many slots can we fit into an allocation of a specific mem_sz */
static inline int
chan_nslots(int item_sz, int mem_sz)
{
	return leqpow2((mem_sz - sizeof(struct __chan_mem)) / item_sz);
}

#endif /* CHAN_PRIVATE_H */
