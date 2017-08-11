#ifndef CRINGBUF_H
#define CRINGBUF_H

#include <string.h>
#ifdef LINUX_TEST
#include <assert.h>
#ifdef CRINGBUF_DEBUG
#define printd(str, args...) printf(str, ##args)
#else
#define printd(str, args...)
#endif
#else
#ifndef REDEFINE_ASSERT
#include <cos_debug.h>
#endif
#define printd(str, args...)
#endif

#ifdef CRINGBUF_CAUTIOUS
/* Take a performance hit for zeroing out quite a bit of memory, and
 * checking it is still zeroed */
#define memsetd(a, b, c) memset(a, b, c)
static inline void
__cringbuf_zeros(char *buf, int amnt)
{
	int i;
	for (i = 0; i < amnt; i++) assert(buf[i] == 0);
}
#else
#define memsetd(a, b, c)
#define __cringbuf_zeros(b, a)
#endif

/***
 * If on a multiprocessor where the producer and consumer are on
 * different cores, it is essential for performance that the head and
 * tail values are on different cache lines to prevent false sharing.
 * Note that the tail is on the same cache line as the data items in
 * the buffer -- this is intentional as the producer both writes to
 * tail and the actual buffer data itself, while the consumer writes
 * only to head.
 */
#define CRINGBUF_ALIGN_TAIL
//#define CRINGBUF_ALIGN_TAIL __attribute__((align(64)))

/* Include assert.h/cos_debug.h before you include this file */

/* shared ring buffer*/
struct __cringbuf {
	int                     head;
	CRINGBUF_ALIGN_TAIL int tail;
	char                    buffer[0];
};
/* local data-structure */
struct cringbuf {
	int                sz; /* total size of the mapping */
	struct __cringbuf *b;
};

static void
cringbuf_init(struct cringbuf *rb, void *buffer, int alloc_sz)
{
	assert(rb && buffer);
	rb->b       = buffer;
	rb->sz      = alloc_sz - sizeof(struct __cringbuf);
	rb->b->head = rb->b->tail = 0;
}

static inline int
cringbuf_empty(struct cringbuf *rb)
{
	assert(rb && rb->b);
	return rb->b->head == rb->b->tail;
}

static inline int
cringbuf_sz(struct cringbuf *rb)
{
	int head, tail;

	assert(rb);
	head = rb->b->head;
	tail = rb->b->tail;
	if (head <= tail)
		return tail - head;
	else
		return rb->sz - (head - tail);
}

static inline int
cringbuf_empty_sz(struct cringbuf *rb)
{
	assert(rb && rb->b);
	return rb->sz - 1 - cringbuf_sz(rb);
}

static inline int
cringbuf_full(struct cringbuf *rb)
{
	assert(rb && rb->b);
	assert(cringbuf_sz(rb) <= (rb->sz - 1));
	return cringbuf_sz(rb) == (rb->sz - 1);
}

/* returns a contiguous extent of active entries (not _all_ used entries) */
static inline char *
cringbuf_active_extent(struct cringbuf *rb, int *len, int amnt)
{
	struct __cringbuf *b;
	int                head, tail;

	assert(rb && rb->b);
	b = rb->b;
	if (cringbuf_empty(rb)) return NULL;
	head = b->head;
	tail = b->tail;
	assert(head != tail);
	if (head < tail)
		*len = tail - head;
	else
		*len = rb->sz - head;
	if (*len > amnt) *len = amnt;

	return &b->buffer[head];
}

static inline char *
cringbuf_inactive_extent(struct cringbuf *rb, int *len, int amnt)
{
	struct __cringbuf *b;
	int                head, tail;

	assert(rb && rb->b);
	b    = rb->b;
	*len = 0;
	if (cringbuf_full(rb)) return NULL;

	head = b->head;
	tail = b->tail;
	if (head <= tail)
		*len = rb->sz - tail;
	else
		*len = head - tail - 1;
	if (*len > amnt) *len = amnt;

	return &b->buffer[tail];
}

/* amnt should be < cringbuf_active_extent */
static inline void
cringbuf_delete(struct cringbuf *rb, int amnt)
{
	int   l = 0, head, nhead;
	char *c;

	assert(rb && rb->b);
	c = cringbuf_active_extent(rb, &l, amnt);
	assert(c && l <= amnt);
	head  = rb->b->head;
	nhead = head + l;
	assert(nhead <= rb->sz);
	if (nhead == rb->sz) nhead = 0;
	rb->b->head = nhead;
}

/* amnt should be <= cringbuf_inactive_extent */
static inline void
cringbuf_add(struct cringbuf *rb, int amnt)
{
	int   l, tail, ntail;
	char *c;

	assert(rb && rb->b);
	c = cringbuf_inactive_extent(rb, &l, amnt);
	assert(c && l <= amnt);
	tail  = rb->b->tail;
	ntail = tail + l;
	assert(ntail <= rb->sz);
	if (ntail == rb->sz) ntail = 0;
	rb->b->tail = ntail;
	assert(ntail != rb->b->head);
	assert(ntail < rb->sz);
}

static int
ringbuf_consume_some(struct cringbuf *rb, char *b, int amnt)
{
	int   l;
	char *t;

	t = cringbuf_active_extent(rb, &l, amnt);
	if (!t) return 0;
	memcpy(b, t, l);
	cringbuf_delete(rb, l);
	memsetd(t, 0, l); /* only for debugging */

	return l;
}

static int
ringbuf_produce_some(struct cringbuf *rb, char *b, int amnt)
{
	int   l;
	char *t;

	t = cringbuf_inactive_extent(rb, &l, amnt);
	if (!t) return 0;
	__cringbuf_zeros(t, l); /* only for debugging */
	memcpy(t, b, l);
	assert(l + rb->b->tail <= rb->sz);
	cringbuf_add(rb, l);
	assert(rb->b->tail < rb->sz);

	return l;
}

/***
 * Consume data from the ring buffer by copying a maximum amount into
 * a separate buffer.  Returns the amount copied.  Think of this as
 * the "read" equivalent.  You should use cringbuf_active_extent and
 * cringbuf_delete if you can to avoid the copy.
 */
static int
cringbuf_consume(struct cringbuf *rb, char *b, int amnt)
{
	int left = amnt;

	printd("consume %d: h %d, tail %d >> ", amnt, rb->b->head, rb->b->tail);
	left -= ringbuf_consume_some(rb, b, amnt);
	if (left) left -= ringbuf_consume_some(rb, b + (amnt - left), left);

	printd("h %d, tail %d\n", rb->b->head, rb->b->tail);

	return amnt - left;
}

static int
cringbuf_produce(struct cringbuf *rb, char *b, int amnt)
{
	int left = amnt;

	printd("produce %d: h %d, tail %d >> ", amnt, rb->b->head, rb->b->tail);
	left -= ringbuf_produce_some(rb, b, amnt);
	if (left) left -= ringbuf_produce_some(rb, b + (amnt - left), left);

	printd("h %d, tail %d\n", rb->b->head, rb->b->tail);

	return amnt - left;
}

#endif
