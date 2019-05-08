/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */
#ifndef CIRQUE_H
#define CIRQUE_H

/* remember to use multi-core locks as these are really single producer, single consumer */
#define CIRQUE_MAX_SZ 4096

#define CIRQUE_PROTOTYPE(name, type)						\
struct cirque_##name {								\
	type wrk[CIRQUE_MAX_SZ];						\
	size_t size;								\
	size_t mask;								\
										\
	volatile long head;							\
	volatile long tail;							\
};										\
										\
static inline void								\
cirque_init_##name(struct cirque_##name *q, size_t sz)				\
{										\
	memset(q, 0, sizeof(struct cirque_##name));				\
										\
	if (sz) {								\
		/* only for size with pow of 2 */				\
		assert(round_to_pow2(sz) == sz);				\
		assert(sz <= CIRQUE_MAX_SZ);					\
	} else {								\
		sz = CIRQUE_MAX_SZ;						\
	}									\
										\
	q->head = q->tail = 0;							\
	q->size = sz;								\
	q->mask = sz - 1;							\
}										\
										\
static inline int								\
cirque_insert_##name(struct cirque_##name *q, type *w)				\
{										\
	long ct = ps_load((unsigned long *)&q->tail); 				\
	long ch = ps_load((unsigned long *)&q->head);				\
										\
	if ((ct == 0 && ch == q->mask) || 					\
	    ((ch + 1) & q->mask) == ct) return -ENOSPC;				\
										\
	ps_mem_fence();								\
	if (!ps_cas((unsigned long *)q->head, ch,				\
		    (ch + 1) & q->mask)) return -EAGAIN;			\
	q->wrk[ch] = *w;							\
										\
	return 0;								\
}										\
										\
static inline int								\
cirque_delete_##name(struct cirque_##name *q, type *w)				\
{										\
	long ct = ps_load((unsigned long *)&q->tail); 				\
	long ch = ps_load((unsigned long *)&q->head);				\
										\
	if (ct >= ch) return -ENOENT;						\
										\
	*w = q->wrk[ct];							\
	if (!ps_cas((unsigned long *)q->tail, ct, 				\
		    (ct + 1) & q->mask)) return -EAGAIN;			\
										\
	return 0;								\
}										\
										\
static inline int								\
cirque_peek_##name(struct cirque_##name *q, type *w)				\
{										\
	long ct = ps_load((unsigned long *)&q->tail); 				\
	long ch = ps_load((unsigned long *)&q->head);				\
										\
	if (ct >= ch) return -ENOENT;						\
										\
	*w = q->wrk[ct];							\
										\
	return 0;								\
}										\
										\
static inline type *								\
cirque_allocptr_##name(struct cirque_##name *q)					\
{										\
	long ct = ps_load((unsigned long *)&q->tail); 				\
	long ch = ps_load((unsigned long *)&q->head);				\
										\
	if ((ct == 0 && ch == q->mask) || 					\
	    ((ch + 1) & q->mask) == ct) return NULL;				\
										\
	ps_mem_fence();								\
	if (!ps_cas((unsigned long *)q->head, ch,				\
		    (ch + 1) & q->mask)) return NULL;				\
										\
	return &q->wrk[ch];							\
}										\
										\
static inline void								\
cirque_freeptr_##name(struct cirque_##name *q)					\
{										\
	long ct = ps_load((unsigned long *)&q->tail); 				\
	long ch = ps_load((unsigned long *)&q->head);				\
										\
	if (ct >= ch) return;							\
										\
	if (ps_cas((unsigned long *)q->tail, ct, (ct + 1) & q->mask)) {		\
		memset(&q->wrk[ct], 0, sizeof(type));				\
	}									\
										\
	return;									\
}										\
										\
static inline type *								\
cirque_peekptr_##name(struct cirque_##name *q)					\
{										\
	long ct = ps_load((unsigned long *)&q->tail); 				\
	long ch = ps_load((unsigned long *)&q->head);				\
										\
	if (ct >= ch) return NULL;						\
										\
	return &q->wrk[ct];							\
}

#endif /* CIRQUE_H */
