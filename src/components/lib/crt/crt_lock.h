#ifndef CRT_LOCK_H
#define CRT_LOCK_H

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
#include <crt_blkpt.h>

struct crt_lock {
	unsigned long owner_blked;
	struct crt_blkpt blkpt;
};

#define CRT_LOCK_OWNER_BLKED_BITS (sizeof(unsigned long) * 8)
#define CRT_LOCK_BLKED_MASK       (1 << (CRT_LOCK_OWNER_BLKED_BITS - 2))
#define CRT_LOCK_BLKED(e)         ((e) &  CRT_LOCK_BLKED_MASK)
#define CRT_LOCK_OWNER(e)         ((e) & ~CRT_LOCK_BLKED_MASK)


/**
 * Initialize a lock. Does *not* allocate memory for it, and assumes
 * that you pass that memory in as an argument.
 *
 * - @l - the lock
 * - @return - `0` on successful initialization,
 *             `!0` if the backing blockpoint cannot be allocated
 */
static inline int
crt_lock_init(struct crt_lock *l)
{
	l->owner_blked = 0;

	return crt_blkpt_init(&l->blkpt);
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
crt_lock_teardown(struct crt_lock *l)
{
	assert(l->owner_blked == 0);
	if (!ps_cas(&l->owner_blked, 0, ~0)) return 1;

	return crt_blkpt_teardown(&l->blkpt);
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
crt_lock_take(struct crt_lock *l)
{
	struct crt_blkpt_checkpoint chkpt;

	while (1) {
		unsigned long owner_blked;

		crt_blkpt_checkpoint(&l->blkpt, &chkpt);

		/* Can we take the lock? */
		if (ps_cas(&l->owner_blked, 0, (unsigned long)cos_thdid())) {
			return;	/* success! */
		}

		/* slowpath: we're blocking! Set the blocked bit, or try again */
		owner_blked = ps_load(&l->owner_blked);
		if (!ps_cas(&l->owner_blked, owner_blked, owner_blked | CRT_LOCK_BLKED_MASK)) continue;

		/* We can't take the lock, have set the block bit, and await release */
		crt_blkpt_wait(&l->blkpt, 0, &chkpt);
	}
}

/**
 * Attempts to take the lock, and returns a value depending on if it
 * takes it, or not.
 *
 * - @l - the `crt_lock`
 * - @return - `0` on successful lock acquisition,
 *             '1' if it is already taken.
 */
static inline int
crt_lock_try_take(struct crt_lock *l)
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
crt_lock_release(struct crt_lock *l)
{
	while (1) {
		unsigned long o_b = ps_load(&l->owner_blked);
		int blked = unlikely(CRT_LOCK_BLKED(o_b) == CRT_LOCK_BLKED_MASK);

		assert(CRT_LOCK_OWNER(o_b) == cos_thdid());
		/*
		 * If this doesn't work, then someone must have set
		 * blocked in the mean time. Try again! Blocked can
		 * only be set once, so this is a bounded loop (can
		 * fire two times, max).
		 */
		if (unlikely(!ps_cas(&l->owner_blked, o_b, 0))) continue;

		/* if there are blocked threads, wake 'em up! */
		if (unlikely(blked)) crt_blkpt_wake(&l->blkpt, 0);

		return;
	}
}

#endif /* CRT_LOCK_H */
