/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT

#include <cos_synchronization.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>

//#define TIMED

struct blocked_thds {
	/* Timed records if this invocation is timed or not */
	unsigned short int thd_id;
	unsigned char timed;
	struct blocked_thds *next, *prev;
};

struct meta_lock {
	u16_t owner;
	spdid_t spd;
	unsigned long lock_id;
	unsigned long long gen_num;
	struct blocked_thds b_thds;

	struct meta_lock *next, *prev;
};

static unsigned long lock_id = 1;
/* Head of the linked list of locks. */
static struct meta_lock *locks;
static volatile unsigned long long generation = 0;

/* From the scheduler: */
extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
extern int timed_event_block(spdid_t spdid, unsigned int microsec);
extern int timed_event_wakeup(spdid_t spdid, unsigned short int thd_id);
extern int sched_block(spdid_t spdid);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);

#define TAKE(spdid) 	if (sched_component_take(spdid)) return -1;
#define RELEASE(spdid)	if (sched_component_release(spdid)) return -1;

static inline struct meta_lock *lock_find(unsigned long lock_id, spdid_t spd)
{
	struct meta_lock *tmp = locks;

	//print("lock_find: lockid %d, spdid %d. %d", lock_id, spd, 0);
	while (tmp) {
		if (tmp->lock_id == lock_id && tmp->spd == spd) {
			return tmp;
		}

		tmp = tmp->next;
	}
	
	return NULL;
}

static int lock_is_thd_blocked(struct meta_lock *ml, unsigned short int thd)
{
	struct blocked_thds *bt;

	for (bt = FIRST_LIST(&ml->b_thds, next, prev) ; bt != &ml->b_thds ; bt = bt->next) {
		if (bt->thd_id == thd) return 1;
	}
	return 0;
}

static struct meta_lock *lock_alloc(spdid_t spd)
{
	struct meta_lock *l = (struct meta_lock*)malloc(sizeof(struct meta_lock));
 	
	if (!l) {
		return NULL;
	}
	/* FIXME: check for lock_id overload */
	l->b_thds.thd_id = 0;
	INIT_LIST(&l->b_thds, next, prev);
	l->lock_id = lock_id++;
	l->owner = 0;
	l->gen_num = 0;
	l->spd = spd;
	l->prev = NULL;
	l->next = locks;
	if (locks) locks->prev = l;
	locks = l;
	
	//print("lock_alloc: lock find %d. %d%d", lock_find(l->lock_id, spd) ? 1 : 0, 0,0);
	return l;
}

static void lock_free(struct meta_lock *l)
{
	if (!l) return;

	if (l->next) l->next->prev = l->prev;
	if (l->prev) l->prev->next = l->next;
	else         locks         = l->next;

	free(l);
}

/* Public functions: */

/* 
 * The problem being solved here is this: T_1 wishes to take the
 * mutex, finds that it is taken by another thread.  It calls into
 * this function, but is preempted by T_2, the lock holder.  The lock
 * is released.  T_1 is switched back to and it invokes this component
 * asking to block till the lock is released.  This component has no
 * way of knowing that the lock already has been released, so we block
 * for no reason in wait for the lock to be "released".  Thus what we
 * do is have the client call the pretake function checking before and
 * after invoking it that the lock is still taken.  We record the
 * generation number in pretake and make sure that it is consistent in
 * take.  This signifies that no release has happened in the interim,
 * and that we really should sleep.
 */
int lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd)
{
	struct meta_lock *ml;
	spdid_t spdid = cos_spd_id();
	int ret = 0;

	TAKE(spdid);
	ml = lock_find(lock_id, spd);
	if (!ml) {
		ret = -1;
		goto done;
	}
	ml->gen_num = generation;
done:
	RELEASE(spdid);
	return ret;
}

int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id, unsigned int microsec)
{
	struct meta_lock *ml;
	spdid_t spdid = cos_spd_id();
	unsigned short int curr = (unsigned short int)cos_get_thd_id();
	struct blocked_thds blocked_desc = {.thd_id = curr};
	int ret = 0;
	
////	print("thread %d from spd %d locking for %d micrseconds.", curr, spdid, microsec);

	TAKE(spdid);

	if (0 == microsec) {
		ret = TIMER_EXPIRED;
		goto error;
	}
	ml = lock_find(lock_id, spd);
	/* tried to access a lock not yet created */
	if (!ml) {
		ret = -1;
		//print("take wtf%d%d%d", 0,0,0);
		goto error;
	}
	assert(!lock_is_thd_blocked(ml, curr));

	if (ml->gen_num != generation) {
		ml->gen_num = generation;
		ret = 1;
		goto error;
	}
	generation++;

	/* Note that we are creating the list of blocked threads from
	 * memory allocated on the individual thread's stacks. */
	INIT_LIST(&blocked_desc, next, prev);
	ADD_LIST(&ml->b_thds, &blocked_desc, next, prev);
	blocked_desc.timed = (TIMER_EVENT_INF != microsec);
	ml->owner = thd_id;

	RELEASE(spdid);

	/* Bypass calling the timed every component if there is an infinite wait */
//	assert(TIMER_EVENT_INF == microsec);
//	assert(!blocked_desc.timed);
	if (TIMER_EVENT_INF == microsec) {
		if (-1 == sched_block(spdid)) return -1;
		/* 
		 * OK, this seems ridiculous but here is the rational: Assume
		 * we are a middle-prio thread, and were just woken by a low
		 * priority thread. We will preempt that thread when woken,
		 * and will continue here.  If a high priority thread is also
		 * waiting on the lock, then we would preempt the low priority
		 * thread while it should wake the high prio thread. With the
		 * following crit sect will switch to the low prio thread that
		 * still holds the component lock.  See the comments in
		 * lock_component_release. 
		 */
		TAKE(spdid);
		RELEASE(spdid);
		ret = 0;
	} else {
		/* ret here will fall through */
		if (-1 == (ret = timed_event_block(spdid, microsec))) return -1;

		/* 
		 * We might have woken from a timeout, which means
		 * that we need to remove this thread from the waiting
		 * list for the lock.
		 */
		TAKE(spdid);
		ml = lock_find(lock_id, spd);
		if (!ml) {
			ret = -1;
			goto error;
		}
		REM_LIST(&blocked_desc, next, prev);
		RELEASE(spdid);
		/* ret is set to the amnt of time we blocked */
	}
	//sched_block_dependency(cos_get_thd_id(), thd);
	return ret;
error:
	RELEASE(spdid);
	return ret;
}

int lock_component_release(spdid_t spd, unsigned long lock_id)
{
	struct meta_lock *ml;
	struct blocked_thds *sent, *bt;
	spdid_t spdid = cos_spd_id();

	TAKE(spdid);

	generation++;
	ml = lock_find(lock_id, spd);
	if (!ml) goto error;
	/* Apparently, lock_take calls haven't been made. */
	if (EMPTY_LIST(&ml->b_thds, next, prev)) {
		goto done;
	}
	sent = bt = FIRST_LIST(&ml->b_thds, next, prev);
	/* Remove all threads from the lock's list */
	REM_LIST(&ml->b_thds, next, prev);
	/* Unblock all waiting threads */
	do {
		struct blocked_thds *next;
		/* This is suboptimal: if we wake a thread with a
		 * higher priority, it will be switched to.  Given we
		 * are holding the component lock here, we should get
		 * switched _back_ to so as to wake the rest of the
		 * components. */
		next = bt->next;
		REM_LIST(bt, next, prev);
		/* Wakeup the way we were put the sleep */
		if (bt->timed) {
			timed_event_wakeup(spdid, bt->thd_id);
		} else {
			sched_wakeup(spdid, bt->thd_id);
		}
		bt = next;
	} while (bt != bt->next);
	/* This is sneaky, so to reiterate: Keep this lock till now so
	 * that if we wake another thread, and it begins execution,
	 * the system will switch back to this thread so that we can
	 * wake up the rest of the waiting threads (one of which might
	 * have the highest priority) */
done:
	RELEASE(spdid);

	return 0;
error:
	RELEASE(spdid);
	return -1;
}

unsigned long lock_component_alloc(spdid_t spd)
{
	struct meta_lock *l;

	l = lock_alloc(spd);
	if (!l) goto error;
	//print("lock id %d, find lock %d. %d", l->lock_id, lock_find(l->lock_id, spd) ? 1 : 0, 0);
 
	return l->lock_id;
error:
	return 0;
}

void lock_component_free(spdid_t spd, unsigned long lock_id)
{
	struct meta_lock *l;
	spdid_t spdid = cos_spd_id();

	if (sched_component_take(spdid)) return;
	l = lock_find(lock_id, spd);
	if (sched_component_release(spdid)) return;

	if (l) lock_free(l);

	return;
}
