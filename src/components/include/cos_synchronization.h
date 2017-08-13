/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_SYNCHONIZATION_H
#define COS_SYNCHONIZATION_H

#define STATIC_ALLOC

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_time.h>

#ifndef assert
#define assert(x)
#endif

union cos_lock_atomic_struct {
	struct {
		volatile u16_t owner;     /* thread id || 0 */
		volatile u16_t contested; /* 0 || 1 */
	} c;
	volatile u32_t v;
} __attribute__((packed, aligned(4)));

typedef struct __attribute__((packed)) {
	volatile union cos_lock_atomic_struct atom;
	u32_t                                 lock_id;
} cos_lock_t;

/* Provided by the synchronization primitive component */
extern unsigned long lock_component_alloc(spdid_t spdid, vaddr_t lock_addr);
extern void          lock_component_free(spdid_t spdid, unsigned long lock_id);

int lock_release_contention(cos_lock_t *l, union cos_lock_atomic_struct *prev_val);
int lock_take_contention(cos_lock_t *l, union cos_lock_atomic_struct *result, union cos_lock_atomic_struct *prev_val,
                         u16_t owner);

static inline int
__cos_cas(unsigned long *target, unsigned long cmp, unsigned long updated, int smp)
{
	if (smp)
		return cos_cas(target, cmp, updated);
	else
		return cos_cas_up(target, cmp, updated);
}

static inline int
__lock_take(cos_lock_t *l, int smp)
{
	union cos_lock_atomic_struct result, prev_val;
	unsigned int                 curr = cos_get_thd_id();
	u16_t                        owner;

	prev_val.c.owner = prev_val.c.contested = 0;
	result.v                                = 0;
	do {
	restart:
		/* Atomically copy the entire 32 bit structure */
		prev_val.v         = l->atom.v;
		owner              = prev_val.c.owner;
		result.c.owner     = curr;
		result.c.contested = 0;
		assert(owner != curr); /* No recursive lock takes allowed */

		/* Contention path: If there is an owner, whom is not
		 * us, go through the motions of blocking on the lock.
		 * This is hopefully the uncommon case. If not, some
		 * structural reconfiguration is probably going to be
		 * needed.  */
		if (unlikely(owner)) {
			int ret;

			ret = lock_take_contention(l, &result, &prev_val, owner);
			if (ret < 0) return ret;
			/* try to take the lock again */
			goto restart;
		}
		assert(result.v == curr);
		/* Commit the new lock value, or try again */
	} while (unlikely(!__cos_cas((unsigned long *)&l->atom.v, prev_val.v, result.v, smp)));
	assert(l->atom.c.owner == curr);

	return 0;
}

static inline int
__lock_release(cos_lock_t *l, int smp)
{
	unsigned int                 curr = cos_get_thd_id();
	union cos_lock_atomic_struct prev_val;

	prev_val.c.owner = prev_val.c.contested = 0;
	do {
		assert(sizeof(union cos_lock_atomic_struct) == sizeof(u32_t));
		prev_val.v = l->atom.v; /* local copy of lock */
		/* If we're here, we better own the lock... */
		if (unlikely(prev_val.c.owner != curr)) BUG();
		if (unlikely(prev_val.c.contested)) {
			return lock_release_contention(l, &prev_val);
		}

		/* The loop is necessary as when read, the lock might
		 * not be contested, but by the time we get here,
		 * another thread might have tried to take it. */
	} while (unlikely(!__cos_cas((unsigned long *)&l->atom, prev_val.v, 0, smp)));
	assert(l->atom.c.owner != curr);

	return 0;
}

/*
 * The hard-coding of the smp values here, with function inlining and
 * constant propagation should remove any costs of having smp
 * conditionals in the code.
 */
static inline int
lock_release(cos_lock_t *l)
{
	return __lock_release(l, 1);
}
/* uni-processor variant for partitioned data-structures */
static inline int
lock_release_up(cos_lock_t *l)
{
	return __lock_release(l, 0);
}
static inline int
lock_take(cos_lock_t *l)
{
	return __lock_take(l, 1);
}
/* uni-processor variant for partitioned data-structures */
static inline int
lock_take_up(cos_lock_t *l)
{
	return __lock_take(l, 0);
}

static unsigned int
lock_contested(cos_lock_t *l)
{
	return l->atom.c.owner;
}

static inline unsigned long
lock_id_alloc(cos_lock_t *l)
{
	return lock_component_alloc(cos_spd_id(), (vaddr_t)l);
}

#define NCACHED_LOCK_IDS 32
extern u32_t __lid_cache[];
extern int   __lid_top;

static inline void
lock_id_put(u32_t lid)
{
	if (__lid_top == NCACHED_LOCK_IDS)
		lock_component_free(cos_spd_id(), lid);
	else
		__lid_cache[__lid_top++] = lid;
}

static inline u32_t
lock_id_get(cos_lock_t *l)
{
	if (__lid_top == 0)
		return lock_id_alloc(l);
	else
		return __lid_cache[--__lid_top];
}

static inline int
lock_init(cos_lock_t *l)
{
	l->lock_id = 0;
	l->atom.v  = 0;

	return 0;
}

static inline unsigned long
lock_static_init(cos_lock_t *l)
{
	lock_init(l);
	l->lock_id = lock_id_get(l);

	return l->lock_id;
}

static inline void
lock_static_free(cos_lock_t *l)
{
	assert(l);
	lock_id_put(l->lock_id);
	lock_init(l);
}

#ifndef STATIC_ALLOC
#include <cos_alloc.h>
cos_lock_t *lock_alloc(void);
void        lock_free(cos_lock_t *l);
#endif

#endif
