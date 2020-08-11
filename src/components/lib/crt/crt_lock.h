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
	unsigned long owner;
	struct crt_blkpt blkpt;
};

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
	l->owner = 0;

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
	assert(l->owner == 0);
	if (!ps_cas(&l->owner, 0, ~0)) return 1;

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
		crt_blkpt_checkpoint(&l->blkpt, &chkpt);

		if (ps_cas(&l->owner, 0, (unsigned long)cos_thdid())) {
			return;	/* success! */
		}
		/* failure: try and block */
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
	if (ps_cas(&l->owner, 0, (unsigned long)cos_thdid())) {
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
	assert(l->owner == cos_thdid());
	l->owner = 0;
	/* if there are blocked threads, wake 'em up! */
	crt_blkpt_trigger(&l->blkpt, 0);
}

#endif /* CRT_LOCK_H */
