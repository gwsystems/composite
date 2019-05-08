#ifndef CRT_BLKPT_H
#define CRT_BLKPT_H

#include <cos_types.h>
#include <ps.h>
#include <sl.h>

/***
 * The event count/block point is an abstraction to synchronize the
 * blocking behavior of different threads on abstract events. The
 * events are usually tied to a specific state of another
 * data-structure (into which the blkpt is embedded).  For example, a
 * lock is taken and released thus generating an event for any
 * blocking threads, or a ring buffer has a data item inserted into
 * it, thus generating an event for any threads waiting for
 * data. Concretely, we want a number of threads to be able to block,
 * and a thread to be able to wake up one, or all of them. The
 * challenge is solving a single race-condition:
 *
 * thd 0: check data-structure, determine the need for blocking and
 *        waiting for an event
 * thd 0: preemption, switching to thd 1
 * thd 1: check data-structure, determine that an event is generated
 * thd 1: call the scheduler, and wake all blocked threads (not
 *        including thd 0 yet)
 * thd 1: preempt, and switch to thd 0
 * thd 0: call scheduler to block
 *
 * The resulting state is that thd 1 should have unblocked thd 0, but
 * due to a race, the thd 0 will be blocked awaiting the *next* event
 * that may never come. Event counts are meant to solve this
 * problem. Traditional systems solve this problem using condition
 * variables and a lock around the scheduling logic, but if you want
 * to decouple the data-structure from the scheduler (e.g. as they are
 * in different modes, or components), this is a fundamental problem.
 *
 * The event count abstraction:
 *
 * Assume the data-structure generating events has at least three
 * states:
 * S0: available
 * S1: unavailable
 * S2: unavailable & subscribed
 *
 * The transitions within the data-structure are:
 * {S0->S1, S1->S0, S1->S2, S2->S0}
 *
 * Every transition into S0 is an abstract *event*. Threads that look
 * at the state of the data-structure, and must block waiting for its
 * state to change, wait for such an event to wakeup.
 *
 * The data-structure must define its own mapping to this state
 * machine. A few examples:
 *
 * Mutexes:
 * S0: Not locked.
 * S1: Locked and held by thread 0.
 * S2: Locked and held by thread 0, and threads 1...N contend the lock
 *
 * Ring buffer (for simplicity, assuming it never fills):
 * S0: data items in ring buffer
 * S1: no data in ring buffer
 * S2: no data in ring buffer, and thread(s) are waiting for data
 *
 * The event counts are used to track the threads that use the
 * data-structure when transitioning from S1->S2 (block thread), when
 * it is in S2 (block additional threads), and when it transitions
 * from S2->S0 (wakeup blocked threads).
 *
 * The event count is used in the following way:
 *
 * S0->S1:
 *     data-structure (DS) operation
 *     E.g. not locked -> locked, or
 *          dequeue from ring with single data item
 *
 * S1->S0:
 *     blkpt_checkpoint(ec) (not used)
 *     data-structure (DS) operation
 *     assert(blkpt_has_blocked(ec) == false) (as we're in S1)
 *     blkpt_trigger(ec) (won't do much as noone is blocked)
 *     E.g. unlock with no contention, or
 *          enqueue with no dequeuing threads
 *
 * S1->S2:
 *     cp = blkpt_checkpoint(ec)
 *     data-structure (DS) operation, determine we need to await event
 *     blkpt_wait(ec, cp)
 *     retry (this is why event counts can be used with lock-free data-structs)
 *     E.g. locked -> contended
 *          empty ring -> waiting for data
 *
 * S2->S0:
 *     data-structure (DS) operation
 *     assert(blkpt_has_blocked(ec) == true) (as we're in S2)
 *     blkpt_trigger(ec) (wake blocked threads!)
 *     E.g. unlock with contention, or
 *          enqueue with dequeuing threads
 *
 * Event count *optimization*:
 *
 * We prevent the race above using an epoch (count) for the events
 * thus the name. However, to avoid rapid wraparound on the epoch, we
 * only increment the epoch when the race condition is possible. That
 * is to say, we only increment the event count when the
 * data-structure has blocked threads. This not only delays
 * wraparound, it also will avoid an atomic instruction for all
 * operations that don't involve blocked threads (a common-case,
 * exemplified by futexes, for example).
 *
 * Usage optimization:
 *
 * Because of the event counter optimization to only use expensive
 * operations when triggering there are blocked threads, the user of
 * this API can trigger whenever transitioning back to S0.
 */

struct crt_blkpt {
	sched_blkpt_id_t  id;
	/* most significant bit specifies blocked thds */
	sched_blkpt_epoch_t epoch_blocked;
};

struct crt_blkpt_checkpoint {
	sched_blkpt_epoch_t epoch_blocked;
};

typedef enum {
	CRT_BLKPT_UNIPROC   = 1, 	/* are the event operations only called on a single core? */
	CRT_BLKPT_CRIT_SECT = 2,	/* is only one thread ever going to trigger at a time? */
} crt_blkpt_flags_t;

#define CRT_BLKPT_EPOCH_BLKED_BITS (sizeof(sched_blkpt_epoch_t) * 8)
#define CRT_BLKPT_BLKED_MASK       (1 << (CRT_BLKPT_EPOCH_BLKED_BITS - 2))
#define CRT_BLKPT_BLKED(e)         ((e) &  CRT_BLKPT_BLKED_MASK)
#define CRT_BLKPT_EPOCH(e)         ((e) & ~CRT_BLKPT_BLKED_MASK)

/* Return != 0 on failure: no ids to allocate */
static inline int
crt_blkpt_init(struct crt_blkpt *blkpt)
{
	sched_blkpt_id_t id;

	id = sched_blkpt_alloc();
	if (id == SCHED_BLKPT_NULL) return -1;

	*blkpt = (struct crt_blkpt){
		.id = id,
		.epoch_blocked = 0
	};

	return 0;
}

static inline int
crt_blkpt_teardown(struct crt_blkpt *blkpt)
{
	return sched_blkpt_free(blkpt->id);
}

/* Internal APIs that must be inlined to remove the branches */
static inline int
__crt_blkpt_atomic_trigger(sched_blkpt_epoch_t *ec, sched_blkpt_epoch_t chkpt, crt_blkpt_flags_t flags)
{
	/*
	 * Assume that the most significant bit is the blocked
	 * indicator. This math might reset it to zero, which we want
	 * to do anyway (as part of CRT_BLKPT_EPOCH).
	 */
	sched_blkpt_epoch_t new = CRT_BLKPT_EPOCH(chkpt + 1);

	/* inlined so that constant propagation will get rid of condition */
	if (flags == CRT_BLKPT_UNIPROC) {
		return ps_upcas(ec, chkpt, new);
	} else {
		return ps_cas(ec, chkpt, new);
	}
	/* TODO: faa for CRT_BLKPT_CRIT_SECT? */
}

/*
 * If we return 1, then the caller will attempt to block, otherwise,
 * return 0 and it will re-check the data-structure assuming that
 * something happened in the mean time.
 */
static inline int
__crt_blkpt_atomic_wait(sched_blkpt_epoch_t *ec, sched_blkpt_epoch_t chkpt, crt_blkpt_flags_t flags)
{
	sched_blkpt_epoch_t cached = ps_load(ec);
	sched_blkpt_epoch_t new    = cached | CRT_BLKPT_BLKED_MASK;
	int ret;

	/*
	 * We are the second or later blocker. Blocked already
	 * set. We're done here.
	 *
	 * It isn't clear if it is better to have the additional
	 * branch here for this to avoid atomic instructions, or to
	 * just always do the atomic instructions and possibly fail.
	 */
	if (cached == new) return 1;

	/* function is inlined so that constant propagation will get rid of condition */
	if (flags == CRT_BLKPT_UNIPROC) {
		ret = ps_upcas(ec, chkpt, new);
	} else {
		ret = ps_cas(ec, chkpt, new);
	}
	if (unlikely(!ret)) {
		/*
		 * CAS failure can mean that 1. another thread
		 * blocked, and set the blocked bit, or 2. an event is
		 * triggered. In the former case, we still want to
		 * block. In the latter case, we want to go back to
		 * the data-structure.
		 */
		return ps_load(ec) == new; /* same epoch with blocked set? == success */
	}

	return 1;
}

/* Trigger an event, waking blocked threads. */
static inline void
crt_blkpt_trigger(struct crt_blkpt *blkpt, crt_blkpt_flags_t flags)
{
	/*
	 * Note that the flags should likely be passed in statically,
	 * as constants. That way they will be inlined the conditions
	 * in the *_atomic_* function will be removed.
	 */
	sched_blkpt_epoch_t saved = ps_load(&blkpt->epoch_blocked);

	/* The optimization: don't increment events if noone's listening */
	if (likely(!CRT_BLKPT_BLKED(saved))) return;

	/* slow(er) path for when we have blocked threads */
	if (!__crt_blkpt_atomic_trigger(&blkpt->epoch_blocked, saved, flags)) {
		/*
		 * Race here between triggering threads. In this case,
		 * someone else already incremented the epoch and
		 * unblocked the threads. Yeah, helping algorithms!
		 */
		return;
	}
	/*
	 * Note that there is a race here. Multiple threads triggering
	 * events might pass different epochs down to the next
	 * level. This is OK as the next level always takes the epoch
	 * = max(epoch, ...) (for some wraparound-aware version of
	 * max).
	 */
	sched_blkpt_trigger(blkpt->id, CRT_BLKPT_EPOCH(saved + 1), 0);
}

/* Wake only a single, specified thread (tracked manually in the data-structure) */
/* void crt_blkpt_trigger_one(struct crt_blkpt *blkpt, crt_blkpt_flags_t flags, cos_thdid_t thdid); */

/*
 * Checkpoint the state of the current event counter. This checkpoint
 * is the one that is active during our operations on the
 * data-structure. If we determine that we want to wait for an event
 * (thus blocking), then the state of the checkpoint will be compared
 * versus the state of the event counter to see if we're working off
 * of outdated information.
 */
static inline void
crt_blkpt_checkpoint(struct crt_blkpt *blkpt, struct crt_blkpt_checkpoint *chkpt)
{
	chkpt->epoch_blocked = ps_load(&blkpt->epoch_blocked);
}

/* Wait for an event. */
static inline void
crt_blkpt_wait(struct crt_blkpt *blkpt, crt_blkpt_flags_t flags, struct crt_blkpt_checkpoint *chkpt)
{
	/*
	 * If blocked is already set, we can try and block
	 * directly. Otherwise, go through and try to atomically set
	 * it. If that fails, then either epoch or blocked has been
	 * updated, so return and try accessing the data-structure
	 * again.
	 */
	if (!CRT_BLKPT_BLKED(chkpt->epoch_blocked) &&
	    !__crt_blkpt_atomic_wait(&blkpt->epoch_blocked, chkpt->epoch_blocked, flags)) return;

	if (unlikely(sched_blkpt_block(blkpt->id, CRT_BLKPT_EPOCH(chkpt->epoch_blocked), 0))) {
		BUG(); 		/* we are using a blkpt id that doesn't exist! */
	}
}

/*
 * Create an execution dependency on the specified thread for,
 * e.g. priority inheritance.
 */
/* void crt_blkpt_wait_dep(struct crt_blkpt *blkpt, crt_blkpt_flags_t flags, struct crt_blkpt_checkpoint *chkpt, cos_thdid_t thdid); */

#endif /* CRT_BLKPT_H */
