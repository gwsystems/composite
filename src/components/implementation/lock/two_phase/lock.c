/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial author: Gabriel Parmer, gabep1@cs.bu.edu, 2008.
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
static volatile unsigned long long generation = 0;
/* Datastructure of blocked thread structures */
COS_VECT_CREATE_STATIC(bthds);

#define TAKE(spdid) 	do { if (sched_component_take(spdid))    return -1; } while (0)
#define RELEASE(spdid)	do { if (sched_component_release(spdid)) return -1; } while (0)

/* 
 * FIXME: to make this predictable (avoid memory allocation in the
 * must-be-predictable case, we should really cos_vect_add_id when we
 * first find out about the possibility of the thread making any
 * invocations.
 */
static struct blocked_thds *bt_get(unsigned short int tid)
{
	struct blocked_thds *bt;

	bt = cos_vect_lookup(&bthds, tid);
	if (NULL == bt) {
		bt = malloc(sizeof(struct blocked_thds));
		if (NULL == bt) return NULL;
		INIT_LIST(bt, next, prev);
		bt->thd_id = tid;
		if (tid != cos_vect_add_id(&bthds, bt, tid)) return NULL;
	}
	return bt;
}

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

	ACT_RECORD(ACT_PRELOCK, spd, lock_id, cos_get_thd_id(), thd);
	TAKE(spdid);
//	lock_print_all();
	ml = lock_find(lock_id, spd);
	if (NULL == ml) {
		ret = -1;
		goto done;
	}
	ml->gen_num = generation;
done:
	RELEASE(spdid);
	return ret;
}

/* 
 * Dependencies here (thus priority inheritance) will NOT be used if
 * you specify a timeout value.
 */
int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id, unsigned int microsec)
{
	struct meta_lock *ml;
	spdid_t spdid = cos_spd_id();
	unsigned short int curr = (unsigned short int)cos_get_thd_id();
	struct blocked_thds blocked_desc = {.thd_id = curr};
	int ret = 0;
	
//	print("thread %d from spd %d locking for %d micrseconds.", curr, spdid, microsec);

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
		//print("take wtf%d%d%d", 0,0,0);
		goto error;
	}
	if (lock_is_thd_blocked(ml, curr)) {
		prints("lock: lock_is_thd_blocked failed in lock_component_take\n");
		goto error;
	}

	/* The calling component needs to retry its user-level lock,
	 * some preemption has caused the generation count to get off,
	 * i.e. we don't have the most up-to-date view of the
	 * lock's state */
	if (ml->gen_num != generation) {
		ml->gen_num = generation;
		ret = 0;
		goto error;
	}
	generation++;

	/* Note that we are creating the list of blocked threads from
	 * memory allocated on the individual thread's stacks. */
	INIT_LIST(&blocked_desc, next, prev);
	ADD_LIST(&ml->b_thds, &blocked_desc, next, prev);
	blocked_desc.timed = (TIMER_EVENT_INF != microsec);
	//ml->owner = thd_id;

	RELEASE(spdid);

	/* Bypass calling the timed every component if there is an infinite wait */
//	assert(TIMER_EVENT_INF == microsec);
//	assert(!blocked_desc.timed);
	if (TIMER_EVENT_INF == microsec) {
		printc("%d: lt %d %d %d\n", (unsigned int)curr, lock_id, thd_id, spd);
		if (-1 == sched_block(spdid, thd_id)) {
			printc("Deadlock including thdids %d -> %d in spd %d, lock id %d.\n", 
			       cos_get_thd_id(), thd_id, spd, (int)lock_id);
			debug_print("BUG: Possible deadlock @ "); 
			assert(0);
			if (-1 == sched_block(spdid, 0)) assert(0);
		}
		printc("%d:\tlt %d\n", (unsigned int)curr, lock_id);
		if (!EMPTY_LIST(&blocked_desc, next, prev)) BUG();
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
		//TAKE(spdid);
		//RELEASE(spdid);

		ACT_RECORD(ACT_WAKEUP, spd, lock_id, cos_get_thd_id(), 0);
		ret = 0;
	} else {
		assert(0);
#ifdef NIL
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
#endif 
	}
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

	ACT_RECORD(ACT_UNLOCK, spd, lock_id, cos_get_thd_id(), 0);
	TAKE(spdid);

	generation++;
	ml = lock_find(lock_id, spd);
	if (!ml) goto error;

	/* Apparently, lock_take calls haven't been made. */
	if (EMPTY_LIST(&ml->b_thds, next, prev)) {
		RELEASE(spdid);
		return 0;
	}
	sent = bt = FIRST_LIST(&ml->b_thds, next, prev);
	/* Remove all threads from the lock's list */
	REM_LIST(&ml->b_thds, next, prev);
	/* Unblock all waiting threads */
	while (1) {
		struct blocked_thds *next;
		u16_t tid;
		int timed;

		/* This is suboptimal: if we wake a thread with a
		 * higher priority, it will be switched to.  Given we
		 * are holding the component lock here, we should get
		 * switched _back_ to so as to wake the rest of the
		 * components. */
		next = FIRST_LIST(bt, next, prev);
		REM_LIST(bt, next, prev);

		ACT_RECORD(ACT_WAKE, spd, lock_id, cos_get_thd_id(), bt->thd_id);

		/* cache locally */
		tid = bt->thd_id;
		timed = bt->timed;

		/* Last node in the list? */
		if (bt == next) {
			/* This is sneaky, so to reiterate: Keep this
			 * lock till now so that if we wake another
			 * thread, and it begins execution, the system
			 * will switch back to this thread so that we
			 * can wake up the rest of the waiting threads
			 * (one of which might have the highest
			 * priority).  We release before we wake the
			 * last as we don't really need the lock
			 * anymore, an it will avoid quite a few
			 * invocations.*/
			RELEASE(spdid);
		}

		/* Wakeup the way we were put to sleep */
		if (timed) {
			assert(0);
#ifdef NIL
			timed_event_wakeup(spdid, tid);
#endif
		} else {
			assert(tid != cos_get_thd_id());
			printc("%d: lr %d %d %d\n", cos_get_thd_id(), lock_id, tid, spd);
			sched_wakeup(spdid, tid);
			printc("%d:\tlr %d\n", cos_get_thd_id(), lock_id);
		}
		if (bt == next) break;
		bt = next;
	}

	return 0;
error:
	RELEASE(spdid);
	return -1;
}

unsigned long lock_component_alloc(spdid_t spd)
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


void cos_init(void *arg)
{
	cos_vect_init_static(&bthds);
}
