/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Updated in 2012, gparmer@gwu.edu: real atomic instructions, and
 * splitting the fast path from the rest.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_synchronization.h>
#include <print.h>

extern int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id);
extern int lock_component_release(spdid_t spd, unsigned long lock_id);
extern int lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd);

/* 
 * Return values: 
 * 0   : try and take the lock again in local memory
 * -ret: return -ret
 */
int
lock_take_contention(cos_lock_t *l, union cos_lock_atomic_struct *result, 
		     union cos_lock_atomic_struct *prev_val, u16_t owner)
{
	int     lock_id = l->lock_id;
	spdid_t spdid   = cos_spd_id();
	int ret;

	if (lock_component_pretake(spdid, lock_id, owner)) return -1;

	/* Must access memory (be volatile) as we want
	 * to detect changes here */
	if (owner != l->atom.c.owner) return 0;
	/* Mark the lock as contested */
	if (!l->atom.c.contested) {
		result->c.contested = 1;
		result->c.owner     = owner;
		if (!cos_cas((unsigned long*)&l->atom.v, prev_val->v, result->v)) return 0;
		assert(l->atom.c.contested);
	}
	/* Note if a 1 is returned, there is a
	 * generation mismatch, and we just want to
	 * try and take the lock again anyway */
	ret = lock_component_take(spdid, lock_id, owner);
	return ret < 0 ? ret : 0;
}

int
lock_release_contention(cos_lock_t *l, union cos_lock_atomic_struct *prev_val)
{
	int lock_id = l->lock_id;
	/* 
	 * This must evaluate to false, as contested
	 * is already set, we are the owner (thus no
	 * other thread should set that),
	 */
	if (unlikely(!cos_cas((unsigned long*)&l->atom, prev_val->v, 0))) BUG();
	if (lock_component_release(cos_spd_id(), lock_id)) return -1;
	return 0;
}

/* 
 * Cache of lock ids for this component so that we don't have to call
 * the lock component for each lock we create.
 */
u32_t __lid_cache[NCACHED_LOCK_IDS] = {};
int   __lid_top = 0;

#ifndef STATIC_ALLOC
cos_lock_t *lock_alloc(void)
{
	cos_lock_t *l = (cos_lock_t*)malloc(sizeof(cos_lock_t));
	if (!l) return 0;

	lock_init(l);
	l->lock_id = lock_id_alloc();
	if (0 == l->lock_id) {
		free(l);
		return 0;
	}

	return l;
}

void lock_free(cos_lock_t *l)
{
	assert(l);
	lock_component_free(cos_spd_id(), l->lock_id);
	free(l);
}
#endif
