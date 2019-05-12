/*
 * Copyright 2019, Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#ifndef CRT_CHAN_H
#define CRT_CHAN_H

/***
 *
 */

#include <cos_component.h>
#include <crt_blkpt.h>
#include <bitmap.h>

struct crt_chan {
	u32_t producer;
	/* If the ring is empty, recving threads will block on this blkpt. */
	struct crt_blkpt empty;
	char _padding1[CACHE_LINE * 2 - (sizeof(struct crt_blkpt) + sizeof(u32_t))];
	u32_t consumer;
	/* If the ring is full, sending thread will block on this blkpt. */
	struct crt_blkpt full;
	char _padding2[CACHE_LINE * 2 - (sizeof(struct crt_blkpt) + sizeof(u32_t))];
	/*
	 * @item_sz is a power of two and corresponds to the
	 * wraparound_mask. The number of data items that the channel
	 * can hold is item_sz - 1. @wraparound_mask = nslots-1 (were
	 * nslots is a power of two)
	 */
	u32_t item_sz, wraparound_mask;
	u32_t nslots;
	/* The memory for the channel. */
	char mem[0];
};

/* produce a  */
#define CRT_CHAN_STATIC_ALLOC(name, type, nslots)		\
struct __crt_chan_envelope_##name {	                        \
        struct crt_chan c;					\
	char mem[nslots * sizeof(type)];			\
} __##name;							\
struct crt_chan *name = &__##name.c

#define CRT_CHAN_TYPE_PROTOTYPES(name, type, nslots)			\
static inline int							\
crt_chan_init_##name(struct crt_chan *c)				\
{ return crt_chan_init(c, sizeof(type), nslots); }			\
static inline void							\
crt_chan_teardown_##name(struct crt_chan *c)				\
{ crt_chan_teardown(c); }						\
static inline int							\
crt_chan_empty_##name(struct crt_chan *c)				\
{ return __crt_chan_empty(c, nslots - 1); }				\
static inline int							\
crt_chan_full_##name(struct crt_chan *c)				\
{ return __crt_chan_full(c, nslots - 1); }				\
static inline int							\
crt_chan_send_##name(struct crt_chan *c, void *item)			\
{									\
	assert(pow2(nslots));						\
	return __crt_chan_send(c, item, nslots - 1, sizeof(type));	\
}									\
static inline int							\
crt_chan_recv_##name(struct crt_chan *c, void *item)			\
{									\
	assert(pow2(nslots));						\
	return __crt_chan_recv(c, item, nslots - 1, sizeof(type));	\
}									\
static inline int							\
crt_chan_async_send_##name(struct crt_chan *c, void *item)		\
{									\
	assert(pow2(nslots));						\
	if (__crt_chan_produce(c, item, nslots - 1, sizeof(type))) return -EAGAIN; \
	return 0;							\
}									\
static inline int							\
crt_chan_async_recv_##name(struct crt_chan *c, void *item)		\
{									\
	assert(pow2(nslots));						\
	if (__crt_chan_consume(c, item, nslots - 1, sizeof(type))) return -EAGAIN; \
	return 0;							\
}

#define CRT_CHANCHAN_PROTOTYPES(nslots) \
CRT_CHAN_TYPE_PROTOTYPES(chan, struct chan *, nslots

static inline unsigned int
__crt_chan_buff_idx(struct crt_chan *c, u32_t v, u32_t wraparound_mask)
{ return v & wraparound_mask; }

static inline int
__crt_chan_full(struct crt_chan *c, u32_t wraparound_mask)
{ return c->consumer == __crt_chan_buff_idx(c, c->producer + 1, wraparound_mask); }

static inline int
__crt_chan_empty(struct crt_chan *c, u32_t wraparound_mask)
{ return c->producer == c->consumer; }

static inline int
__crt_chan_produce(struct crt_chan *c, void *d, u32_t wraparound_mask, u32_t sz)
{
	if (__crt_chan_full(c, wraparound_mask)) return 1;
	memcpy(c->mem + (__crt_chan_buff_idx(c, c->producer, wraparound_mask) * sz), d, sz);
	c->producer++;

	return 0;
}

static inline int
__crt_chan_consume(struct crt_chan *c, void *d, u32_t wraparound_mask, u32_t sz)
{
	void *ret;

	if (__crt_chan_empty(c, wraparound_mask)) return 1;
	memcpy(d, c->mem + (__crt_chan_buff_idx(c, c->consumer, wraparound_mask) * sz), sz);
	c->consumer++;

	return 0;
}

/**
 * The next two functions pass all of the variables in via arguments,
 * so that we can use them for constant propagation along with
 * inlining to get rid of the general memcpy code.
 */
static inline int
__crt_chan_send(struct crt_chan *c, void *item, u32_t wraparound_mask, u32_t item_sz)
{
	while (1) {
		struct crt_blkpt_checkpoint chkpt;

		crt_blkpt_checkpoint(&c->full, &chkpt);
		if (!__crt_chan_produce(c, item, wraparound_mask, item_sz)) {
			/* success! */
			crt_blkpt_trigger(&c->empty, 0);
			break;
		}
		crt_blkpt_wait(&c->full, 0, &chkpt);
	}

	return 0;
}

static inline int
__crt_chan_recv(struct crt_chan *c, void *item, u32_t wraparound_mask, u32_t item_sz)
{
	while (1) {
		struct crt_blkpt_checkpoint chkpt;

		crt_blkpt_checkpoint(&c->empty, &chkpt);
		if (!__crt_chan_consume(c, item, wraparound_mask, item_sz)) {
			/* success! */
			crt_blkpt_trigger(&c->full, 0);
			break;
		}
		crt_blkpt_wait(&c->empty, 0, &chkpt);
	}

	return 0;
}


/*
 * We need to know how much to malloc? This function returns that
 * requirement. It assumes (and checks) that @slots is a power of two.
 */
static inline int
crt_chan_mem_sz(int item_sz, int slots)
{
	assert(pow2(slots));

	return sizeof(struct crt_chan) + item_sz * slots;
}

/* How many slots can we fit into an allocation of a specific mem_sz */
static inline int
crt_chan_nslots(int item_sz, int mem_sz)
{
	return leqpow2((mem_sz - sizeof(struct crt_chan)) / item_sz);
}

static inline int
crt_chan_init(struct crt_chan *c, int item_sz, int slots)
{
	assert(pow2(slots));
	if (crt_blkpt_init(&c->empty)) return -1;
	if (crt_blkpt_init(&c->full)) return -1;
	c->nslots  = slots;
	c->item_sz = item_sz;
	c->wraparound_mask = slots - 1; /* slots is a pow2 */

	return 0;
}

static inline void
crt_chan_teardown(struct crt_chan *c)
{
	crt_blkpt_teardown(&c->empty);
	crt_blkpt_teardown(&c->full);
}

/* User-facing send and receive APIs: */

static inline int
crt_chan_send(struct crt_chan *c, void *item)
{
	return __crt_chan_send(c, item, c->wraparound_mask, c->item_sz);
}

static inline int
crt_chan_recv(struct crt_chan *c, void *item)
{
	return __crt_chan_recv(c, item, c->wraparound_mask, c->item_sz);
}

static inline int
crt_chan_async_send(struct crt_chan *c, void *item)
{
	if (__crt_chan_produce(c, item, c->wraparound_mask, c->item_sz)) return -EAGAIN;
	return 0;
}

static inline int
crt_chan_async_recv(struct crt_chan *c, void *item)
{
	if (__crt_chan_consume(c, item, c->wraparound_mask, c->item_sz)) return -EAGAIN;
	return 0;
}

#endif /* CRT_CHAN_H */
