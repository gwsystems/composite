#ifndef INPLACE_RING_H
#define INPLACE_RING_H

#include <cos_debug.h>
#include <cos_types.h>
#include <ck_ring.h>

/* useful for shared memory in-place rings. */

/* for built-in data-types */
#define INPLACE_RING_BUILTIN(name, type)		\
struct inplace_##name {					\
	type val;					\
};							\
							\
CK_RING_PROTOTYPE(name, inplace_##name);		\
							\
static inline struct ck_ring *				\
inplace_ring_##name(vaddr_t addr, size_t sz)		\
{							\
	return (struct ck_ring *)(addr + sz		\
		- sizeof(struct ck_ring));		\
}							\
							\
static inline struct ck_ring *				\
inplace_ring_init_##name(vaddr_t addr, size_t sz)	\
{							\
	struct ck_ring *cr = 				\
		inplace_ring_##name(addr, sz);		\
	size_t cap = ((sz - sizeof(struct ck_ring)) /	\
			sizeof(type));			\
							\
	/* ck_ring number of elements  gotta be ^2 */	\
	assert(cap % 2 == 0);				\
							\
	ck_ring_init(cr, cap);				\
							\
	return cr;					\
}							\
							\
static inline bool					\
inplace_ring_enq_spsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_enqueue_spsc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_spsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_dequeue_spsc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_enq_spmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_enqueue_spmc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_spmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_dequeue_spmc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_enq_mpsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_enqueue_mpsc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_mpsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_dequeue_mpsc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_enq_mpmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_enqueue_mpmc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_mpmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, type *d)			\
{							\
	return ck_ring_dequeue_mpmc_##name(r, 		\
		(struct inplace_##name *)addr,		\
		(struct inplace_##name *)d);		\
}

/* for user-defined data-types */
#define INPLACE_RING_USERDEF(name, type)		\
CK_RING_PROTOTYPE(name, type);				\
							\
static inline struct ck_ring *				\
inplace_ring_##name(vaddr_t addr, size_t sz)		\
{							\
	return (struct ck_ring *)(addr + sz		\
		- sizeof(struct ck_ring));		\
}							\
							\
static inline struct ck_ring *				\
inplace_ring_init_##name(vaddr_t addr, size_t sz)	\
{							\
	struct ck_ring *cr = 				\
		inplace_ring_##name(addr, sz);		\
	size_t cap = ((sz - sizeof(struct ck_ring)) /	\
			sizeof(struct type));		\
							\
	/* ck_ring number of elements  gotta be ^2 */	\
	assert(cap % 2 == 0);				\
							\
	ck_ring_init(cr, cap);				\
							\
	return cr;					\
}							\
							\
static inline bool					\
inplace_ring_enq_spsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_enqueue_spsc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_spsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_dequeue_spsc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_enq_spmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_enqueue_spmc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_spmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_dequeue_spmc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_enq_mpsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_enqueue_mpsc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_mpsc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_dequeue_mpsc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_enq_mpmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_enqueue_mpmc_##name(r, 		\
		(struct type *)addr, d);		\
}							\
							\
static inline bool					\
inplace_ring_deq_mpmc_##name(vaddr_t addr, 		\
	struct ck_ring *r, struct type *d)		\
{							\
	return ck_ring_dequeue_mpmc_##name(r, 		\
		(struct type *)addr, d);		\
}

#endif
