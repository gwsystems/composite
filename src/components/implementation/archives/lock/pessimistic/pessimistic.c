/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial author: Gabriel Parmer, gabep1@cs.bu.edu, 2008.
 *
 * The George Washington University, 2010.
 * Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#define COS_FMT_PRINT

//#include <cos_synchronization.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <print.h>
#include <cos_vect.h>

#include <lock.h>

#include <sched.h>
#include <timed_blk.h>

//#define ACT_LOG
#ifdef ACT_LOG
#define ACT_LOG_LEN 32
#define ACTION_TIMESTAMP 1

typedef enum {
	ACT_PRELOCK,
	ACT_LOCK,
	ACT_UNLOCK,
	ACT_WAKE,
	ACT_WAKEUP
} action_t;
typedef enum {
	ACT_SPDID,
	ACT_LOCK_ID,
	ACT_T1,
	ACT_T2,
	ACT_ITEM_MAX
} action_item_t;
#define NUM_ACT_ITEMS ACT_ITEM_MAX

#include <cos_actlog.h>
#define ACT_RECORD(a, s, l, t1, t2)					\
	do {								\
		unsigned long as[] = {s, l, t1, t2};			\
		action_record(a, as, NULL);				\
	} while (0)
#else
#define ACT_RECORD(a, s, l, t1, t2)
#endif

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
	struct blocked_thds b_thds;
	unsigned long long gen_num;

	struct meta_lock *next, *prev;
};

static volatile unsigned long lock_id = 1;
/* Head of the linked list of locks. */
static struct meta_lock STATIC_INIT_LIST(locks, next, prev);

#define TAKE(spdid) 	if (sched_component_take(spdid))    return -1;
#define RELEASE(spdid)	if (sched_component_release(spdid)) return -1;

static inline struct meta_lock *lock_find(unsigned long lock_id, spdid_t spd)
{
	struct meta_lock *tmp;

	for (tmp = FIRST_LIST(&locks, next, prev) ; 
	     tmp != &locks ; 
	     tmp = FIRST_LIST(tmp, next, prev)) {
		if (tmp->lock_id == lock_id && tmp->spd == spd) {
			return tmp;
		}
		assert(tmp != FIRST_LIST(tmp, next, prev));
	}
	
	return NULL;
}

static void lock_print_all(void)
{
	struct meta_lock *ml;

	for (ml = FIRST_LIST(&locks, next, prev) ; 
	     ml != &locks ; // && ml != FIRST_LIST(ml, next, prev) ; 
	     ml = FIRST_LIST(ml, next, prev)) {
		printc("lock @ %p (next %p, prev %p), id %d, spdid %d\n", 
		       ml, ml->next, ml->prev, (unsigned int)ml->lock_id, ml->spd);
	}
	prints("\n");
}

static struct meta_lock *lock_alloc(spdid_t spd)
{
	struct meta_lock *l;
	struct meta_lock *snd, *lst;
 	
	l = (struct meta_lock*)malloc(sizeof(struct meta_lock));
	if (!l) return NULL;
	l->b_thds.thd_id = 0;
	INIT_LIST(&(l->b_thds), next, prev);
	/* FIXME: check for lock_id overflow */
	l->lock_id = lock_id++;
	l->owner = 0;
	l->gen_num = 0;
	l->spd = spd;
	INIT_LIST(l, next, prev);
	assert(&locks != l);
	snd = FIRST_LIST(&locks, next, prev);
	lst = LAST_LIST(&locks, next, prev);
	(l)->next = (&locks)->next;
	(l)->prev = (&locks); 
	(&locks)->next = (l); 
	(l)->next->prev = (l);
	assert(FIRST_LIST(&locks, next, prev) == l);
	assert(LAST_LIST(l, next, prev) == &locks);
	if (lst != &locks) {
		assert(LAST_LIST(&locks, next, prev) == lst);
		assert(FIRST_LIST(lst, next, prev) == &locks);
	}
	assert(FIRST_LIST(l, next, prev) == snd && LAST_LIST(snd, next, prev) == l);
	
//	lock_print_all();
	return l;
}

static void lock_free(struct meta_lock *l)
{
	assert(l && l != &locks);
	REM_LIST(l, next, prev);
	free(l);
}

/* Public functions: */

/* Pessimistic lock doesn't do pretakes */
int lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd)
{
	BUG();
	return 0;
}

/* 
 * Dependencies here (thus priority inheritance) will NOT be used if
 * you specify a timeout value.
 */
int lock_component_take(spdid_t spd, unsigned long lock_id, 
			unsigned short int thd_id /* ignored */, unsigned int microsec) 
{
	struct meta_lock *ml;
	spdid_t spdid = cos_spd_id();
	unsigned short int curr = (unsigned short int)cos_get_thd_id();
	unsigned short int owner;
	struct blocked_thds blocked_desc = {.thd_id = curr};
	int ret = 0;
	
	ACT_RECORD(ACT_LOCK, spd, lock_id, cos_get_thd_id(), thd_id);
	TAKE(spdid);

	if (0 == microsec) {
		ret = TIMER_EXPIRED;
		goto error;
	}
	ml = lock_find(lock_id, spd);
	/* tried to access a lock not yet created */
	if (!ml) {
		ret = -1;
		goto error;
	}

	/* Lock not taken? */
	owner = ml->owner;
	if (!owner) {
		ml->owner = curr;
		RELEASE(spdid);
		goto done;
	}

	/* Note that we are creating the list of blocked threads from
	 * memory allocated on the individual thread's stacks. */
	INIT_LIST(&blocked_desc, next, prev);
	ADD_LIST(&ml->b_thds, &blocked_desc, next, prev);
	blocked_desc.timed = (TIMER_EVENT_INF != microsec);
	//ml->owner = thd_id;

	RELEASE(spdid);

	/* Bypass calling the timed every component if there is an infinite wait */
	if (TIMER_EVENT_INF == microsec) {
		if (-1 == sched_block(spdid, owner)) BUG();
		if (!EMPTY_LIST(&blocked_desc, next, prev)) BUG();

		ACT_RECORD(ACT_WAKEUP, spd, lock_id, cos_get_thd_id(), 0);
		ret = 0;
	} else {
		/* ret here will fall through.  We do NOT use the
		 * dependency here as I can't think through the
		 * repercussions */
		if (-1 == (ret = timed_event_block(spdid, microsec))) return ret;

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

		ACT_RECORD(ACT_WAKEUP, spd, lock_id, cos_get_thd_id(), 0); 
		/* ret is set to the amnt of time we blocked */
	}
done:
	return ret;
error:
	RELEASE(spdid);
	return ret;
}

int lock_component_release(spdid_t spd, unsigned long lock_id)
{
	struct meta_lock *ml;
	struct blocked_thds *bt;
	spdid_t spdid = cos_spd_id();
	u16_t tid;
	int timed;

	ACT_RECORD(ACT_UNLOCK, spd, lock_id, cos_get_thd_id(), 0);
	TAKE(spdid);

	ml = lock_find(lock_id, spd);
	if (!ml) goto error;

	assert(ml->owner == cos_get_thd_id());
	/* Give up the lock */
	ml->owner = 0;

	/* Apparently, lock_take calls haven't been made. */
	if (EMPTY_LIST(&ml->b_thds, next, prev)) {
		RELEASE(spdid);
		return 0;
	}
	bt = FIRST_LIST(&ml->b_thds, next, prev);
	REM_LIST(bt, next, prev);
	
	ACT_RECORD(ACT_WAKE, spd, lock_id, cos_get_thd_id(), bt->thd_id);
	
	/* cache locally */
	tid = bt->thd_id;
	timed = bt->timed;
	ml->owner = tid;
	RELEASE(spdid);
	
	/* Wakeup the way we were put to sleep */
	if (timed) {
		timed_event_wakeup(spdid, tid);
	} else {
		assert(tid != cos_get_thd_id());
		sched_wakeup(spdid, tid);
	}

	/* FIXME, BROKEN: The above logic will probably not work with
	 * multiple threads waiting on the lock with dependencies from
	 * all of them to this thread.  They are expecting this thread
	 * to release them all, not just the first one. */

	return 0;
error:
	RELEASE(spdid);
	return -1;
}

unsigned long lock_component_alloc(spdid_t spd, vaddr_t laddr)
{
	struct meta_lock *l;
	spdid_t spdid = cos_spd_id();

	TAKE(spdid);
	l = lock_alloc(spd);
	RELEASE(spdid);
	
	if (!l) return 0;
 	return l->lock_id;
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

#ifdef ACT_LOG
unsigned long *lock_stats(spdid_t spdid, unsigned long *stats)
{
	struct action *a;
	int sz = (NUM_ACT_ITEMS + 2) * sizeof(unsigned long);

	if (!cos_argreg_buff_intern((char*)stats, sz)) {
		return NULL;
	}
	
	if (NULL == (a = action_report())) return NULL;
	memcpy(stats, a, sz);
	return stats;
}

int lock_stats_len(spdid_t spdid)
{
	return NUM_ACT_ITEMS + 2;
}
#else 

unsigned long *lock_stats(spdid_t spdid, unsigned long *stats) { return NULL; }
int lock_stats_len(spdid_t spdid) { return 0; }

#endif
