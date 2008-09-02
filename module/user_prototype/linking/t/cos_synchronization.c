/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "cos_component.h"
#include "cos_synchronization.h"

/* 
 * Return the amount of time that have elapsed since the request was
 * made if we get the lock, _or_ TIMER_EXPIRED if we did not get the
 * lock, but at least microsec microseconds have passed.
 */
int lock_take_timed(cos_lock_t *l, unsigned int microsec)
{
	struct cos_lock_atomic_struct result;
	volatile u32_t *result_ptr;
	u32_t new_val = 0, prev_val;
	unsigned int curr = cos_get_thd_id(), owner;
	spdid_t spdid = cos_spd_id();
	unsigned int elapsed_time = 0;

	result_ptr = (volatile u32_t *)&result;
	do {
		int ret;
restart:
		//print("%d%d%d",8,8,8);
		/* Atomically copy the entire 32 bit structure */
		prev_val = *result_ptr = *(volatile u32_t *)&l->atom;
		owner = result.owner;

		/* If there is an owner, whom is not us, go through
		 * the motions of blocking on the lock.  This is
		 * hopefully the uncommon case. If not, some
		 * structural reconfiguration is probably going to be
		 * needed.  */
		if (unlikely(owner && owner != curr)) {
			if (0 == microsec) return TIMER_EXPIRED;

			if (lock_component_pretake(spdid, l->lock_id, owner)) {
				/* lock_id not valid */
				return -1;
			}
			/* Must access memory (be volatile) as we want
			 * to detect changes here */
			if (owner != l->atom.owner) goto restart;

			/* Mark the lock as contested */
			if (!l->atom.contested) {
				result.contested = 1;
				new_val = *result_ptr;
				//print("blocking: %x. %d%d", new_val, 0,0);
				if (cos_cmpxchg(&l->atom, prev_val, new_val) != new_val) {
					/* start the whole process
					 * again if the lock status
					 * has changed */
					goto restart;
				}
				/* A preemption at this point makes this assert incorrect */
				//assert(l->atom.contested);
			}
			/* Note if a 1 is returned, there is a
			 * generation mismatch, and we just want to
			 * try and take the lock again anyway */
			//print("pretake %d%d%d", 0,0,0);
			ret = lock_component_take(spdid, l->lock_id, owner, microsec);
			if (ret != -1 && ret != TIMER_EXPIRED) {
				int diff = microsec - ret;

				microsec = diff > 0 ? diff : 0;
				elapsed_time += ret;
			} else {
				return ret;
			}
			//print("posttake %d%d%d", 0,0,0);

			/* try to take the lock again */
			goto restart;
		}
		/* If we are the current owner, progress increasing
		 * the recursion count */
		else if (owner == curr) {
			result.rec_cnt++;
			assert(result.rec_cnt < 255);
		} else /* !owner */ {	
			assert(result.contested == 0 && result.rec_cnt == 0);
			result.owner = curr;
		}

		new_val = *result_ptr;
		//print("taking lock: %x. %d%d", new_val, 0,0);
		/* Commit the new lock value, or try again */
	} while (unlikely(cos_cmpxchg(&l->atom, prev_val, new_val) != new_val));

	assert(l->atom.owner == curr);

	return elapsed_time;
}

int lock_take(cos_lock_t *l) 
{
	/* 
	 * Don't return the return value as the caller doesn't care
	 * about timing for this fn.  If there is an error, however,
	 * pass that down.
	 */
	return (-1 == lock_take_timed(l, TIMER_EVENT_INF)) ? -1 : 0;
}

int lock_release(cos_lock_t *l) {
	unsigned int curr = cos_get_thd_id();
	struct cos_lock_atomic_struct result;
	volatile u32_t *result_ptr;
	u32_t new_val, prev_val;

	result_ptr = (volatile u32_t*)&result;
	do {
		prev_val = *result_ptr = *(volatile u32_t *)&l->atom;
		assert(result.owner == curr);
		
		if (result.rec_cnt == 0) {
			result.owner = 0;
			if (result.contested) {
				result.contested = 0;
				new_val = *result_ptr;
				//print("releasing contested lock: %x. %d%d", new_val, 0,0);
				/* This must be true, as contested is
				 * already set, we are the owner (thus
				 * no other thread should set that),
				 * and rec_cnt can only change by us
				 * taking it, which can't happen
				 * here. */
				assert(cos_cmpxchg(&l->atom, prev_val, new_val) == new_val);
				
				if (lock_component_release(cos_spd_id(), l->lock_id)) {
					/* Lock doesn't exist */
					return -1;
				}
				return 0;
			}
		} else {
			result.rec_cnt--;
		}

		/* The loop is necessary as when read, the lock might
		 * not be contested, but by the time we get here,
		 * another thread might have tried to take it. */
		new_val = *result_ptr;
	} while (cos_cmpxchg(&l->atom, prev_val, new_val) != new_val);

	return 0;
}

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
