#ifndef SYNC_LOCK_H
#define SYNC_LOCK_H

/***
 * Simple blocking lock. Uses blockpoints to enable the blocking and
 * waking of contending threads.
 *
 * **TODO**:
 *
 * - Add dependency specification for PI.
 * - Add optional adaptive spinning and non-preemptivity -- generally, multicore awareness
 * - Thorough testing.
 */

#include <cos_component.h>
#include <sync_blkpt.h>

struct sync_lock {
	unsigned long owner_blked;
	struct sync_blkpt blkpt;
};

#define SYNC_LOCK_OWNER_BLKED_BITS (sizeof(unsigned long) * 8)
#define SYNC_LOCK_BLKED_MASK       (1UL << (SYNC_LOCK_OWNER_BLKED_BITS - 2))
#define SYNC_LOCK_BLKED(e)         ((e) &  SYNC_LOCK_BLKED_MASK)
#define SYNC_LOCK_OWNER(e)         ((e) & ~SYNC_LOCK_BLKED_MASK)


/**
 * Initialize a lock. Does *not* allocate memory for it, and assumes
 * that you pass that memory in as an argument.
 *
 * - @l - the lock
 * - @return - `0` on successful initialization,
 *             `!0` if the backing blockpoint cannot be allocated
 */
static inline int
sync_lock_init(struct sync_lock *l)
{
	l->owner_blked = 0;

	return sync_blkpt_init(&l->blkpt);
}

/**
 * Teardown and delete the lock. Note that this does *not* manipulate
 * or free the lock's memory. This API is high-level and general, and
 * must allow locks to be embedded in structures, or defined globally.
 *
 * @precondition - The lock is not taken.
 *
 * - @l - the lock
 * - @return - `0` on success, and
 *             `!0` if the lock is taken.
 */
static inline int
sync_lock_teardown(struct sync_lock *l)
{
	assert(l->owner_blked == 0);
	if (!ps_cas(&l->owner_blked, 0, ~0)) return 1;

	return sync_blkpt_teardown(&l->blkpt);
}

/**
 * Take the lock.
 *
 * @precondition - we have *not* already taken the lock. Recursive
 * locks not allowed currently.
 *
 * - @l - the lock
 */
static inline void
sync_lock_take(struct sync_lock *l)
{
	struct sync_blkpt_checkpoint chkpt;

	while (1) {
		unsigned long owner_blked;

		sync_blkpt_checkpoint(&l->blkpt, &chkpt);

		owner_blked = ps_load(&l->owner_blked);
		/* Can we take the lock? */
		if (ps_cas(&l->owner_blked, 0, (unsigned long)cos_thdid())) {
			return;	/* success! */
		}

		/* slowpath: we're blocking! Set the blocked bit, or try again */
		if (!ps_cas(&l->owner_blked, owner_blked, owner_blked | SYNC_LOCK_BLKED_MASK)) continue;

		/* We can't take the lock, have set the block bit, and await release */
		sync_blkpt_wait(&l->blkpt, 0, &chkpt);
	}
}

/**
 * Attempts to take the lock, and returns a value depending on if it
 * takes it, or not.
 *
 * - @l - the `sync_lock`
 * - @return - `0` on successful lock acquisition,
 *             '1' if it is already taken.
 */
static inline int
sync_lock_try_take(struct sync_lock *l)
{
	if (ps_cas(&l->owner_blked, 0, (unsigned long)cos_thdid())) {
		return 0;	/* success! */
	} else {
		return 1;
	}
}

/**
 * Release the lock.
 *
 * @precondition: we must have previously taken the lock.
 *
 * - @l - the lock
 */
static inline void
sync_lock_release(struct sync_lock *l)
{
	while (1) {
		unsigned long o_b = ps_load(&l->owner_blked);
		int blked = unlikely(SYNC_LOCK_BLKED(o_b) == SYNC_LOCK_BLKED_MASK);

		assert(SYNC_LOCK_OWNER(o_b) == cos_thdid());
		/*
		 * If this doesn't work, then someone must have set
		 * blocked in the mean time. Try again! Blocked can
		 * only be set once, so this is a bounded loop (can
		 * fire two times, max).
		 */
		if (unlikely(!ps_cas(&l->owner_blked, o_b, 0))) continue;

		/* if there are blocked threads, wake 'em up! */
		if (unlikely(blked)) sync_blkpt_wake(&l->blkpt, 0);

		return;
	}
}

#endif /* SYNC_LOCK_H */
