/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* NEARLY COMPLETELY UNTESTED! */

#include <cos_component.h>
#include <cos_synchronization.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>

extern int timed_event_block(spdid_t spdinv, unsigned int amnt);
extern int timed_event_wakeup(spdid_t spdinv, unsigned short int thd_id);

/* Must be a power of 2 (as I want to avoid modulus in mbox_inc) */
#define MBOX_SIZE  512
#define MBOX_MASK  (MBOX_SIZE-1)
#define MBOX_INC(x) ((x + 1) & MBOX_MASK)

struct mbox_entry {
	void *data;
};

struct blocked_thds {
	unsigned short int tid;
	struct blocked_thds *next, *prev;
};

typedef unsigned long mboxid_t;
static mboxid_t cnt = 1;
/*
 * The mbox structure is laid out as such (data specified by filled
 * boxes):
 *         tail                  head
 *	    |			  |
 *	    \			  /
 * +---+---+-+-+---+---+---+---+-+-+---+----------+---+
 * |   |   | | |...|...|...|...| | |   |   ....   |   |
 * +---+---+---+---+---+---+---+---+---+----------+---+
 * 
 * (artist mode is great)
 */
struct mbox {
	mboxid_t id;
	spdid_t spdid;
	int head, tail;
	cos_lock_t lock;
	/* positive values indicate # of thds blocked on empty,
	 * negative indicates # blocked on full */
	int blocked;
	struct blocked_thds bthds;
	struct mbox_entry values[MBOX_SIZE];
	struct mbox *next, *prev;
};
/* Waste of space.  Sometime I'll rewrite the list ops */
struct mbox mboxes = {.id = 0, .spdid = 0, .next = &mboxes, .prev = &mboxes};

static void mbox_init(struct mbox *mb)
{
	cos_memset(mb, 0, sizeof(struct mbox));
	mb->head = 1;
	mb->tail = 0;
	INIT_LIST(mb, next, prev);
	mb->id = cnt++;
	mb->spdid = cos_spd_id();
	ADD_LIST(&mboxes, mb, next, prev);
	lock_init(&mb->lock);
	mb->bthds.tid = 0;
	INIT_LIST(&mb->bthds, next, prev);
}

static struct mbox *lookup_mbox(mboxid_t id, spdid_t spdid)
{
	struct mbox *tmp = FIRST_LIST(&mboxes, next, prev);

	while (tmp != &mboxes) {
		if (tmp->id == id && tmp->spdid == spdid) {
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

static inline int mbox_full(struct mbox *mb)
{
	return (mb->head == mb->tail);
}

static inline int mbox_empty(struct mbox *mb)
{
	return (mb->head == (mb->tail + 1));
}

static inline int mbox_blocked_on_full(struct mbox *mb)
{
	return mb->blocked < 0;
}

static inline int mbox_blocked_on_empty(struct mbox *mb)
{
	return mb->blocked > 0;
}
/* Mailbox lock should be taken before calling: */
static int mbox_insert(struct mbox *mb, void **data)
{
	struct mbox_entry *entry;

	assert(mb->head >= 0 && mb->head < MBOX_SIZE);
	assert(mb->tail >= 0 && mb->tail < MBOX_SIZE);

	if (mbox_full(mb)) {
		return -1;
	}
	entry = &mb->values[mb->head];
	entry->data = *data;
	mb->head = MBOX_INC(mb->head);

	/* 
	 * Now that we've added an item, see if there are threads
	 * blocked on the mbox being empty (waiting for data.)
	 */
	if (mbox_blocked_on_empty(mb)) {
		struct blocked_thds *other;
		
		mb->blocked--;
		assert(!EMPTY_LIST(&mb->bthds, next, prev));
		other = FIRST_LIST(&mb->bthds, next, prev);
		REM_LIST(other, next, prev);
		timed_event_wakeup(cos_spd_id(), other->tid);
	}

	return 0;
}

static int mbox_remove(struct mbox *mb, void **data)
{
	struct mbox_entry *entry;

	assert(mb->head >= 0 && mb->head < MBOX_SIZE);
	assert(mb->tail >= 0 && mb->tail < MBOX_SIZE);

	/* Nothing in the mailbox: no mail! */
	if (mbox_empty(mb)) {
		return -1;
	}
	mb->tail = MBOX_INC(mb->tail);
	entry = &mb->values[mb->tail];
	*data = entry->data;

	/* 
	 * Now that we've removed an item, see if there are threads
	 * blocked on the mbox being full.
	 */
	if (mbox_blocked_on_full(mb)) {
		struct blocked_thds *other;
		
		mb->blocked++;
		assert(!EMPTY_LIST(&mb->bthds, next, prev));
		other = FIRST_LIST(&mb->bthds, next, prev);
		REM_LIST(other, next, prev);
		timed_event_wakeup(cos_spd_id(), other->tid);
	}

	return 0;
}

#define POSITIVE(x) (x < 0 ? 0 : x)
#define UPDATE_TIME(usec, blocked) \
	(TIMER_EVENT_INF == (unsigned long)usec ? TIMER_EVENT_INF : POSITIVE(usec - blocked))

/* 
 * This structure exists to abstract away the enqueue/dequeue specific
 * information from the generic function for enqueueing or dequeueing
 */
struct q_deq_ops {
	int  (*mbox_op)(struct mbox *, void **data);
	void (*adjust_blocked)(struct mbox *);
	void (*counteradjust_blocked)(struct mbox *);
	/* second_try will be called without the mailbox lock */
	int  (*second_try)(struct mbox *);
};

static void enqueue_adjust_blocked(struct mbox *mb)
{
	assert(mb->blocked <= 0);
	mb->blocked--;
}

static int enqueue_second_try(struct mbox *mb)
{
	return !mbox_full(mb);
}

static void dequeue_adjust_blocked(struct mbox *mb)
{
	assert(mb->blocked >= 0);
	mb->blocked++;
}

static int dequeue_second_try(struct mbox *mb)
{
	return !mbox_empty(mb);
}

struct q_deq_ops enqueue_ops = {
	.mbox_op = mbox_insert,
	.adjust_blocked = enqueue_adjust_blocked,
	.counteradjust_blocked = dequeue_adjust_blocked,
	.second_try = enqueue_second_try
};

struct q_deq_ops dequeue_ops = {
	.mbox_op = mbox_remove,
	.adjust_blocked = dequeue_adjust_blocked,
	.counteradjust_blocked = enqueue_adjust_blocked,
	.second_try = dequeue_second_try
};

/* 
 * Language problems in this function: 1) Too difficult to provide one
 * implementation of this function with small functional differences
 * for two call sites, 2) failure semantics are difficult as we want
 * to have code reuse for 2 different failure sites, additionally
 * failure (expiration) might not really be failure (to post/get from
 * the mbox) if we were dequeued by another thread, thus the failure
 * path might branch back into the main code path, 3) its too easy to
 * forget to release the lock, or do other necessary upkeep (inc/dec
 * the blocked count), and 4) bt must consciously be stack allocated
 * here and we hope that this is valid; some verification would be
 * reassuring, but an escape analysis won't actually help as it does
 * seem to escape.
 */
static int mbox_q_deq(struct q_deq_ops *ops, spdid_t spdid, mboxid_t id, 
		      void **data, unsigned int microsec)
{
	/* FIXME: should be locking on the global structure */
	struct mbox *mb;
	int time;
	/* 
	 * assume that bt is only modified 1) in this thread, or 2)
	 * when the mail box's lock is taken.  
	 */
	struct blocked_thds bt;

	mb = lookup_mbox(id, spdid);
	if (!mb) {
		return -1;
	}

	/* Take the mailbox's lock */
	if (TIMER_EXPIRED == (time = lock_take_timed(&mb->lock, microsec))) {
		return TIMER_EXPIRED;
	}
	microsec = UPDATE_TIME((int)microsec, (int)time);

	/* Try and en/de-queue from the mailbox and loop while we can't */
	while (ops->mbox_op(mb, data)) {
		int elapsed;

		/* Prepare to wait for data to become available, or for the mbox to flush */
		bt.tid = cos_get_thd_id();
		INIT_LIST(&bt, next, prev);
		ADD_LIST(&mb->bthds, &bt, next, prev);
		ops->adjust_blocked(mb);
		lock_release(&mb->lock);

		/* having just released the lock, someone else might
		 * have removed/added items from/to the mbox */
		if (ops->second_try(mb)) goto try_again;

		assert(0);	/* express block in ticks */
		/* Lock is released, and we are going to wait */
		if (0 == microsec ||
		    TIMER_EXPIRED ==
		    (elapsed = timed_event_block(cos_spd_id(), microsec))) {
			microsec = UPDATE_TIME((int)microsec, (int)elapsed);
			goto expired;
		}
		microsec = UPDATE_TIME((int)microsec, (int)elapsed);
try_again:
		/* OK, now take the lock again. */
		if (0 == microsec ||
		    TIMER_EXPIRED ==
		    (elapsed = lock_take_timed(&mb->lock, microsec))) {
			microsec = UPDATE_TIME((int)microsec, (int)elapsed);
			goto expired;
		}
		microsec = UPDATE_TIME((int)microsec, (int)elapsed);
	}
	lock_release(&mb->lock);
	return 0;
	
expired:
	lock_take(&mb->lock);
	/* If we are still on the mbox list as blocked, since we have
	 * the lock, noone has removed us with the intention to wake
	 * us up. */
	if (!EMPTY_LIST(&bt, next, prev)) {
		ops->counteradjust_blocked(mb);
		REM_LIST(&bt, next, prev);
	} else {
		lock_release(&mb->lock);
		/* we expired, but also were dequeued to be serviced.
		 * another thread woke us up as the mailbox was posted
		 * or removed from.  This is horrible: jumping into a
		 * scope, but there is little other option */
		goto try_again;
	}
	lock_release(&mb->lock);
	return TIMER_EXPIRED;
}

int mbox_enqueue(spdid_t spdid, mboxid_t id, void *data, unsigned int microsec)
{
	void **d;
	*d = data;
	return mbox_q_deq(&enqueue_ops, spdid, id, d, microsec);
}

int mbox_dequeue(spdid_t spdid, mboxid_t id, void **data, int microsec)
{
	return mbox_q_deq(&dequeue_ops, spdid, id, data, microsec);
}

mboxid_t mbox_alloc(spdid_t spdid)
{
	struct mbox *mb;

	mb = (struct mbox *)malloc(sizeof(struct mbox));
	if (!mb) {
		return 0;
	}
	mbox_init(mb);

	return mb->id;
}

void mbox_free(spdid_t spdid, mboxid_t id)
{
	struct mbox *mb = lookup_mbox(id, spdid);

	if (mb) {
		REM_LIST(mb, next, prev);
		free(mb);
	}
	return;
}
