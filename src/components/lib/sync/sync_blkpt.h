#ifndef SYNC_BLKPT_H
#define SYNC_BLKPT_H

#include <cos_types.h>
#include <ps.h>
#include <sched.h>

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

struct sync_blkpt {
	sched_blkpt_id_t  id;
	/* most significant bit specifies blocked thds */
	sched_blkpt_epoch_t epoch_blocked;
};

struct sync_blkpt_checkpoint {
	sched_blkpt_epoch_t epoch_blocked;
};

typedef enum {
	SYNC_BLKPT_UNIPROC   = 1, 	/* are the event operations only called on a single core? */
	SYNC_BLKPT_CRIT_SECT = 2,	/* is only one thread ever going to trigger at a time? */
} sync_blkpt_flags_t;

#define SYNC_BLKPT_EPOCH_BLKED_BITS (sizeof(sched_blkpt_epoch_t) * 8)
#define SYNC_BLKPT_BLKED_MASK       (1UL << (SYNC_BLKPT_EPOCH_BLKED_BITS - 2))
#define SYNC_BLKPT_BLKED(e)         ((e) &  SYNC_BLKPT_BLKED_MASK)
#define SYNC_BLKPT_EPOCH(e)         ((e) & ~SYNC_BLKPT_BLKED_MASK)

/**
 * If a blockpoint has already been allocated, we can use it
 * here. This does *not* allocate anything, and just uses an existing
 * blockpoint.
 */
static inline void
sync_blkpt_init_w_id(struct sync_blkpt *blkpt, sched_blkpt_id_t id)
{
	*blkpt = (struct sync_blkpt){
		.id = id,
		.epoch_blocked = 0
	};

	return;
}

/**
 * Initialize the blockpoint, and allocate the scheduler's blockpoint.
 *
 * @return - `!0` on failure: no ids to allocate
 */
static inline int
sync_blkpt_init(struct sync_blkpt *blkpt)
{
	sched_blkpt_id_t id;

	id = sched_blkpt_alloc();
	if (id == SCHED_BLKPT_NULL) return -1;

	sync_blkpt_init_w_id(blkpt, id);

	return 0;
}

/**
 * Deallocate the blkpt.
 *
 * - @return - `0` on success, `!0` if somehow the blockpoint's id got out
 *   of sync with the scheduler.
 */
static inline int
sync_blkpt_teardown(struct sync_blkpt *blkpt)
{
	return sched_blkpt_free(blkpt->id);
}

/* Internal APIs that must be inlined to remove the branches */

/*
 * Return `1` if the cas is successful, and `0` if it isn't successful
 * (and we likely need to do a retry).
 */
static inline int
__sync_blkpt_atomic_trigger(sched_blkpt_epoch_t *ec, sched_blkpt_epoch_t chkpt, sync_blkpt_flags_t flags)
{
	/*
	 * Assume that the most significant bit is the blocked
	 * indicator. This math might reset it to zero, which we want
	 * to do anyway (as part of SYNC_BLKPT_EPOCH).
	 */
	sched_blkpt_epoch_t new = SYNC_BLKPT_EPOCH(chkpt + 1);

	/* inlined so that constant propagation will get rid of condition */
	if (flags == SYNC_BLKPT_UNIPROC) {
		return ps_upcas(ec, chkpt, new);
	} else {
		return ps_cas(ec, chkpt, new);
	}
	/* TODO: faa for SYNC_BLKPT_CRIT_SECT? */
}

/*
 * If we return 1, then the caller will attempt to block, otherwise,
 * return 0 and it will re-check the data-structure assuming that
 * something happened in the mean time.
 */
static inline int
__sync_blkpt_atomic_wait(sched_blkpt_epoch_t *ec, sched_blkpt_epoch_t chkpt, sync_blkpt_flags_t flags)
{
	sched_blkpt_epoch_t cached = ps_load(ec);
	sched_blkpt_epoch_t new    = chkpt | SYNC_BLKPT_BLKED_MASK;
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
	if (flags == SYNC_BLKPT_UNIPROC) {
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

/**
 * Trigger an event, waking blocked threads. Will only wake blocked
 * threads if 1. the blocked bit is set in the blkpt (which means that
 * you have be careful of the race between when the data-structure is
 * observed to require blocking, and when the blocking thread sets the
 * block bit), or 2. if you explicitly tell the API that there are
 * blocked threads (external to the blkpt).
 *
 * - @blkpt - the blockpoint including the (blocked x epoch)
 * - @id -    the id to use for the blockpoint (it might be separate from
 *            the blkpt in some cases).
 * - @blked - If we know due to context *outside* of the blockpoint
 *            that a thread is blocked, we pass `1` here. Otherwise,
 *            pass `0`, and the normal blockpoint lock (and blocked
 *            bit) are used.
 * - @flags - Flags to modify the blockpoint's behavior.
 */
static inline void
sync_blkpt_id_activate(struct sync_blkpt *blkpt, sched_blkpt_id_t id, int blocked, sync_blkpt_flags_t flags)
{
	/*
	 * Note that the flags should likely be passed in statically,
	 * as constants. That way they will be inlined the conditions
	 * in the *_atomic_* function will be removed.
	 */
	sched_blkpt_epoch_t saved = ps_load(&blkpt->epoch_blocked);

	/* The optimization: don't increment events if noone's listening */
	if (!blocked && likely(!SYNC_BLKPT_BLKED(saved))) return;

	/* slow(er) path for when we have blocked threads */
	if (!__sync_blkpt_atomic_trigger(&blkpt->epoch_blocked, saved, flags)) {
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
	sched_blkpt_trigger(id, SYNC_BLKPT_EPOCH(saved + 1), 0);
}

/**
 * Trigger an event on the blockpoint. This will *only* attempt to
 * wake up threads if they have been previously
 * `sync_blkpt_blocking`ed, thus blocking is tracked in the
 * blockpoint itself. See the `chan` library for examples of this.
 */
static inline void
sync_blkpt_id_trigger(struct sync_blkpt *blkpt, sched_blkpt_id_t id, sync_blkpt_flags_t flags)
{
	sync_blkpt_id_activate(blkpt, id, 0, flags);
}

static inline void
sync_blkpt_trigger(struct sync_blkpt *blkpt, sync_blkpt_flags_t flags)
{
	sync_blkpt_id_trigger(blkpt, blkpt->id, flags);
}

/**
 * Wake up blocked threads on a blocked point. We have observed
 * blocked threads as tracked by the data-structure. See the
 * `sync_lock` for an example of this. This API should *not* be used
 * with the `sync_blkpt_blocking` API.
 */
static inline void
sync_blkpt_id_wake(struct sync_blkpt *blkpt, sched_blkpt_id_t id, sync_blkpt_flags_t flags)
{
	sync_blkpt_id_activate(blkpt, id, 1, flags);
}

static inline void
sync_blkpt_wake(struct sync_blkpt *blkpt, sync_blkpt_flags_t flags)
{
	sync_blkpt_id_wake(blkpt, blkpt->id, flags);
}

/* Wake only a single, specified thread (tracked manually in the data-structure) */
/* void sync_blkpt_trigger_one(struct sync_blkpt *blkpt, sync_blkpt_flags_t flags, cos_thdid_t thdid); */

/**
 * Checkpoint the state of the current event counter. This checkpoint
 * is the one that is active during our operations on the
 * data-structure. If we determine that we want to wait for an event
 * (thus blocking), then the state of the checkpoint will be compared
 * versus the state of the event counter to see if we're working off
 * of outdated information.
 *
 * - @blkpt - the blockpoint to checkpoint
 * - @chkpt - the memory to take the checkpoint into
 */
static inline void
sync_blkpt_checkpoint(struct sync_blkpt *blkpt, struct sync_blkpt_checkpoint *chkpt)
{
	chkpt->epoch_blocked = ps_load(&blkpt->epoch_blocked);
}

/**
 * Set the "blocked" status of the blockpoint. This is used *separate*
 * from `sync_blkpt_wait` only if the data-structure we're waiting for
 * doesn't track if threads are blocked on it or not. We can re-check
 * the data-structure after setting block for if we should indeed
 * block. This avoids the race when we check the data-structure, find
 * we should block, and then before we set the blkpt to "blocked", the
 * triggering thread goes through, and doesn't actually call the
 * scheduler's trigger, thus *losing the wakeup*.
 *
 * - @blkpt  - the blockpoint
 * - @id     - blockpoint id
 * - @flags  - optional flags
 * - @return - `1` if the atomic operation has failed, and `0`
 *             otherwise.
 */
static inline int
sync_blkpt_id_blocking(struct sync_blkpt *blkpt, sched_blkpt_id_t id, sync_blkpt_flags_t flags, struct sync_blkpt_checkpoint *chkpt)
{
	/* only set blocked if it isn't already */
	if (SYNC_BLKPT_BLKED(chkpt->epoch_blocked)) return 0;
	return !__sync_blkpt_atomic_wait(&blkpt->epoch_blocked, chkpt->epoch_blocked, flags);
}

static inline int
sync_blkpt_blocking(struct sync_blkpt *blkpt, sync_blkpt_flags_t flags, struct sync_blkpt_checkpoint *chkpt)
{
	return sync_blkpt_id_blocking(blkpt, blkpt->id, flags, chkpt);
}

/**
 * Wait for an event. The @precondition here is that we've already
 * indicated that we're blocking. This could have been either through
 * calling `sync_blkpt_blocking`, or using a data-structure-specific
 * indicator of blocked threads.
 *
 * - @blkpt  - the blockpoint
 * - @id     - blockpoint id
 * - @flags  - optional flags
 * - @chkpt  - the previously taken checkpoint
 */
static inline void
sync_blkpt_id_wait(struct sync_blkpt *blkpt, sched_blkpt_id_t id, sync_blkpt_flags_t flags, struct sync_blkpt_checkpoint *chkpt)
{
	if (unlikely(sched_blkpt_block(id, SYNC_BLKPT_EPOCH(chkpt->epoch_blocked), 0))) {
		BUG(); 		/* we are using a blkpt id that doesn't exist! */
	}
}

static inline void
sync_blkpt_wait(struct sync_blkpt *blkpt, sync_blkpt_flags_t flags, struct sync_blkpt_checkpoint *chkpt)
{
	sync_blkpt_id_wait(blkpt, blkpt->id, flags, chkpt);
}

/*
 * Create an execution dependency on the specified thread for,
 * e.g. priority inheritance.
 */
/* void sync_blkpt_wait_dep(struct sync_blkpt *blkpt, sync_blkpt_flags_t flags, struct sync_blkpt_checkpoint *chkpt, cos_thdid_t thdid); */

#endif /* SYNC_BLKPT_H */
