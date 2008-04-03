/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "cos_synchronization.h"
#include "cos_component.h"
#include "cos_alloc.h"

struct blocked_thds {
	unsigned short int thd_id;
	struct blocked_thds *next;
};

struct meta_lock {
	cos_lock_t lock;
	unsigned short int max_gen;
	spdid_t spd;
	struct blocked_thds *b_thds;

	struct meta_lock *next, *prev;
};

static unsigned long lock_id = 1;
/* Head of the linked list of locks. */
static struct meta_lock *locks;

static inline struct meta_lock *lock_find(unsigned long lock_id, spdid_t spd)
{
	struct meta_lock *tmp = locks;

	while (tmp) {
		if (tmp->lock.lock_id == lock_id && tmp->spd == spd) {
			return tmp;
		}

		tmp = tmp->next;
	}
	
	return NULL;
}

static struct meta_lock *lock_alloc(spdid_t spd)
{
	unsigned long id = lock_id++;
	struct meta_lock *l = (struct meta_lock*)malloc(sizeof(struct meta_lock));
	
	if (!l) {
		lock_id--;
		return NULL;
	}
	/* FIXME: check for lock_id overload */

	l->lock.lock_id = id;
	l->lock.generation = 0;
	l->lock.take_thd = 0;
	l->max_gen = 0;
	l->spd = spd;
	l->prev = NULL;

	l->next = locks;
	if (locks) locks->prev = l;
	locks = l;
	
	return l;
}

static void lock_free(struct meta_lock *l)
{
	if (!l) return;

	if (l->next) l->next->prev = l->prev;

	if (l->prev) l->prev->next = l->next;
	else         locks = l->next;

	free(l);
}

static inline void update_curr_gen(unsigned short int gen, struct meta_lock *ml)
{
	unsigned short int curr_gen = ml->lock.generation, max_gen = ml->max_gen;
	/* We are relying on short int overflow here */
	unsigned short int diff = thd_gen - curr_gen, mdiff = max_gen - curr_gen;

	if (diff <= mdiff) {
		ml->lock.generation = gen;
	} /* otherwise: wtf, mate */
}

/* Is the thread's notion of generation less than the current one? */
static inline int is_locked_gen(unsigned short int thd_gen, struct meta_lock *ml)
{
	unsigned short int curr_gen = ml->lock.generation, max_gen = ml->max_gen;
	/* We are relying on short int overflow here */
	unsigned short int diff = thd_gen - curr_gen, mdiff = max_gen - curr_gen;

	return (diff <= mdiff);
}

int lock_component_take(spdid_t spd, unsigned short int thd, 
			unsigned short int gen, unsigned long lock_id)
{
	struct blocked_thds *thd;
	struct meta_lock *ml;

	ml = lock_find(spd, lock_id);
	/* tried to access a lock not yet created */
	if (!ml) return -1;

	/* 
	 * If we have a thread that got the lock a while ago, and has
	 * an old generation for it, then let it immediately retry.
	 * If it really got its lock a LONG time ago, and it does
	 * really need to block here, it will simply re-run the lock
	 * take code, and come back here with an updated generation.
	 */
	if (!is_locked_gen(gen, ml)) {
		return 0;
	} 

	thd = (struct blocked_thds*)malloc(sizeof(struct blocked_thds));
	/* if we're out of memory, I suppose we won't be handling any
	 * more requests */
	if (!thd) return -1;
	
	thd->thd_id = (unsigned short int)cos_get_thd_id();
	thd->next = ml->b_thds;
	ml->b_thds = thd;

	/* we are waking up the thread holding the lock, so incriment
	 * the max possible gen that lock can be at */
	ml->max_gen++;
	//sched_block_dependency(cos_get_thd_id(), thd);

	return 0;
}

int lock_component_release(spdid_t spd, unsigned short int gen, unsigned long lock_id)
{
	struct meta_lock *ml;
	struct blocked_thds *bt;

	ml = lock_find(spd, lock_id);
	update_curr_gen(gen, ml);
	bt = ml->b_thds;
	ml->b_thds = NULL;

	/* Unblock all waiting threads */
	while (bt) {
		struct blocked_thds *t = bt->next;

		//sched_unblock(bt->thd_id);
		free(bt);
		bt = t;
	}

	return 0;
}

unsigned long lock_component_alloc(spdid_t spd)
{
	struct meta_lock *l = lock_alloc(spd);

	if (!l) return 0;
 
	return l->lock.lock_id;
}

void lock_component_free(spdid_t spd, unsigned long lock_id)
{
	lock_free(lock_find(spd, lock_id));

	return;
}
