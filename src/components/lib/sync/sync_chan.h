/*
 * Copyright 2019, Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#ifndef SYNC_CHAN_H
#define SYNC_CHAN_H

/***
 * A single producer, single consumer ring buffer. Optimized to be
 * wait free in that case that only asynchronous operations are used.
 * Supports synchronous operations in which case the producer/consumer
 * are blocked when the ring is full/empty. This assumes a
 * power-of-two ring size to avoid the overheads of division for
 * wraparound.
 */

#include <cos_component.h>
#include <sync_blkpt.h>

/**
 * We attempt to make this relatively fast. We do this by:
 *
 * 1. Making sure that the cache line that the producer modifies to
 *    track the tail is on a separate cache line from the head that
 *    the consumer modifies. Note that this *does not* remove cache
 *    coherency traffic as we'll have at least cache-line access for
 *    1. the head, 2. the tail, and 3. the data itself. The first two
 *    are necessary to determine full/empty status. TODO: This could
 *    be further optimized by using a header per item in the queue,
 *    thus limiting coherency to only that cache-line.
 *
 * 2. The memory of the ring is inlined into the memory for these
 *    data-structures.
 */
struct sync_chan {
	u32_t producer;
	/* If the ring is empty, recving threads will block on this blkpt. */
	struct sync_blkpt empty;
	char _padding1[CACHE_LINE * 2 - (sizeof(struct sync_blkpt) + sizeof(u32_t))];

	u32_t consumer;
	/* If the ring is full, sending thread will block on this blkpt. */
	struct sync_blkpt full;
	char _padding2[CACHE_LINE * 2 - (sizeof(struct sync_blkpt) + sizeof(u32_t))];
	/*
	 * @item_sz is a power of two and corresponds to the
	 * wraparound_mask. The number of data items that the channel
	 * can hold is item_sz - 1. @wraparound_mask = nslots-1 (where
	 * nslots is a power of two)
	 */
	u32_t item_sz, wraparound_mask;
	u32_t nslots;
	/* The memory for the channel. */
	char mem[0];
};

/* produce a  */
#define SYNC_CHAN_STATIC_ALLOC(name, type, nslots)		\
struct __sync_chan_envelope_##name {	                        \
        struct sync_chan c;					\
	char mem[nslots * sizeof(type)];			\
} __##name;							\
struct sync_chan *name = &__##name.c

#define SYNC_CHAN_TYPE_PROTOTYPES(name, type, nslots)			\
static inline int							\
sync_chan_init_##name(struct sync_chan *c)				\
{									\
	assert(pow2(nslots));						\
	return sync_chan_init(c, sizeof(type), nslots);			\
}									\
static inline void							\
sync_chan_teardown_##name(struct sync_chan *c)				\
{ sync_chan_teardown(c); }						\
static inline int							\
sync_chan_empty_##name(struct sync_chan *c)				\
{ return __sync_chan_empty(c, nslots - 1); }				\
static inline int							\
sync_chan_full_##name(struct sync_chan *c)				\
{ return __sync_chan_full(c, nslots - 1); }				\
static inline int							\
sync_chan_send_##name(struct sync_chan *c, void *item)			\
{ return __sync_chan_send(c, item, nslots - 1, sizeof(type)); }		\
static inline int							\
sync_chan_recv_##name(struct sync_chan *c, void *item)			\
{ return __sync_chan_recv(c, item, nslots - 1, sizeof(type)); }		\
static inline int							\
sync_chan_async_send_##name(struct sync_chan *c, void *item)		\
{									\
	if (__sync_chan_produce(c, item, nslots - 1, sizeof(type))) return -EAGAIN; \
	return 0;							\
}									\
static inline int							\
sync_chan_async_recv_##name(struct sync_chan *c, void *item)		\
{									\
	if (__sync_chan_consume(c, item, nslots - 1, sizeof(type))) return -EAGAIN; \
	return 0;							\
}

#define SYNC_CHANCHAN_PROTOTYPES(nslots)				\
	SYNC_CHAN_TYPE_PROTOTYPES(chan, struct chan *, nslots)

static inline unsigned int
__sync_chan_buff_idx(struct sync_chan *c, u32_t v, u32_t wraparound_mask)
{ return v & wraparound_mask; }

static inline int
__sync_chan_full(struct sync_chan *c, u32_t wraparound_mask)
{ return __sync_chan_buff_idx(c, c->consumer, wraparound_mask) == __sync_chan_buff_idx(c, c->producer + 1, wraparound_mask); }

static inline int
__sync_chan_empty(struct sync_chan *c, u32_t wraparound_mask)
{ return c->producer == c->consumer; }

static inline int
__sync_chan_produce(struct sync_chan *c, void *d, u32_t wraparound_mask, u32_t sz)
{
	if (__sync_chan_full(c, wraparound_mask)) return 1;
	memcpy(c->mem + (__sync_chan_buff_idx(c, c->producer, wraparound_mask) * sz), d, sz);
	c->producer++;

	return 0;
}

static inline int
__sync_chan_consume(struct sync_chan *c, void *d, u32_t wraparound_mask, u32_t sz)
{
	void *ret;

	if (__sync_chan_empty(c, wraparound_mask)) return 1;
	memcpy(d, c->mem + (__sync_chan_buff_idx(c, c->consumer, wraparound_mask) * sz), sz);
	c->consumer++;

	return 0;
}

/**
 * The next two functions pass all of the variables in via arguments,
 * so that we can use them for constant propagation along with
 * inlining to get rid of the general memcpy code.
 */
static inline int
__sync_chan_send(struct sync_chan *c, void *item, u32_t wraparound_mask, u32_t item_sz)
{
	while (1) {
		struct sync_blkpt_checkpoint chkpt;

		sync_blkpt_checkpoint(&c->full, &chkpt);
		if (!__sync_chan_produce(c, item, wraparound_mask, item_sz)) {
			/* success! */
			sync_blkpt_trigger(&c->empty, 0);
			break;
		}
		if (sync_blkpt_blocking(&c->full, 0, &chkpt)) continue;
		if (!__sync_chan_full(c, wraparound_mask)) continue;
		sync_blkpt_wait(&c->full, 0, &chkpt);
	}

	return 0;
}

static inline int
__sync_chan_recv(struct sync_chan *c, void *item, u32_t wraparound_mask, u32_t item_sz)
{
	while (1) {
		struct sync_blkpt_checkpoint chkpt;

		sync_blkpt_checkpoint(&c->empty, &chkpt);
		if (!__sync_chan_consume(c, item, wraparound_mask, item_sz)) {
			/* success! */
			sync_blkpt_trigger(&c->full, 0);
			break;
		}
		if (sync_blkpt_blocking(&c->empty, 0, &chkpt)) continue;
		if (!__sync_chan_empty(c, wraparound_mask)) continue;
		sync_blkpt_wait(&c->empty, 0, &chkpt);
	}

	return 0;
}


/*
 * We need to know how much to malloc? This function returns that
 * requirement. It assumes (and checks) that @slots is a power of two.
 */
static inline int
sync_chan_mem_sz(int item_sz, int slots)
{
	assert(pow2(slots));

	return sizeof(struct sync_chan) + item_sz * slots;
}

/* How many slots can we fit into an allocation of a specific mem_sz */
static inline int
sync_chan_nslots(int item_sz, int mem_sz)
{
	return leqpow2((mem_sz - sizeof(struct sync_chan)) / item_sz);
}

static inline int
sync_chan_init(struct sync_chan *c, int item_sz, int slots)
{
	assert(pow2(slots));
	if (sync_blkpt_init(&c->empty)) return -1;
	if (sync_blkpt_init(&c->full)) return -1;
	c->nslots  = slots;
	c->item_sz = item_sz;
	c->wraparound_mask = slots - 1; /* slots is a pow2 */

	return 0;
}

static inline void
sync_chan_teardown(struct sync_chan *c)
{
	sync_blkpt_teardown(&c->empty);
	sync_blkpt_teardown(&c->full);
}

/* User-facing send and receive APIs: */

static inline int
sync_chan_send(struct sync_chan *c, void *item)
{
	return __sync_chan_send(c, item, c->wraparound_mask, c->item_sz);
}

static inline int
sync_chan_recv(struct sync_chan *c, void *item)
{
	return __sync_chan_recv(c, item, c->wraparound_mask, c->item_sz);
}

static inline int
sync_chan_async_send(struct sync_chan *c, void *item)
{
	if (__sync_chan_produce(c, item, c->wraparound_mask, c->item_sz)) return -EAGAIN;
	return 0;
}

static inline int
sync_chan_async_recv(struct sync_chan *c, void *item)
{
	if (__sync_chan_consume(c, item, c->wraparound_mask, c->item_sz)) return -EAGAIN;
	return 0;
}

#endif /* SYNC_CHAN_H */
