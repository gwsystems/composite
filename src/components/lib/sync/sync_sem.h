#ifndef SYNC_SEM_H
#define SYNC_SEM_H

/***
 * Counting semaphore. Uses blockpoints to enable the blocking and
 * waking of contending threads for counting resources.
 *
 * **TODO**:
 *
 * - Add dependency specification for PI.
 * - Add optional adaptive spinning and non-preemptivity -- generally, multicore awareness
 * - Thorough testing.
 */

#include <cos_component.h>
#include <sync_blkpt.h>

struct sync_sem {
	unsigned long     rescnt;
	struct sync_blkpt blkpt;
};

#define SYNC_SEM_OWNER_BLKED_BITS  (sizeof(unsigned long) * 8)
#define SYNC_SEM_ZERO              (~0UL >> (SYNC_SEM_OWNER_BLKED_BITS / 2))

/**
 * Initialize a semaphore. Does *not* allocate memory for it, and assumes
 * that you pass that memory in as an argument.
 *
 * - @s - the semaphore
 * - @resnum - the number of the resources. `0` is equivalent to a mutex.
 * - @return - `0` on successful initialization,
 *             `!0` if the backing blockpoint cannot be allocated
 */
static inline int
sync_sem_init(struct sync_sem *s, unsigned long resnum)
{
	s->rescnt = SYNC_SEM_ZERO + resnum;

	return sync_blkpt_init(&s->blkpt);
}

/**
 * Teardown and delete the semaphore. Note that this does *not* manipulate
 * or free the semaphore's memory. This API is high-level and general, and
 * must allow locks to be embedded in structures, or defined globally.
 *
 * @precondition - None, can be directly tear down. All open handles are
 *                 invalidated at once.
 *
 * - @s - the semaphore
 * - @return - `0` on success; always successful.
 */
static inline int
sync_sem_teardown(struct sync_sem *s)
{
	/* Shouldn't tear down the semaphore while there are blocked threads. */
	assert(s->rescnt >= SYNC_SEM_ZERO);

	return sync_blkpt_teardown(&s->blkpt);
}

/**
 * Take a semaphore. You may only take one at a time.
 *
 * @precondition - none.
 *
 * - @s - the semaphore
 */
static inline void
sync_sem_take(struct sync_sem *s)
{
	struct sync_blkpt_checkpoint chkpt;

	while (1) {
		unsigned long rescnt;

		sync_blkpt_checkpoint(&s->blkpt, &chkpt);

		rescnt = ps_load(&s->rescnt);
		/*
		 * Attempt to take a count and return; any changes
		 * deserve a "retry". Note this will update even when
		 * multiple threads are able to take the semaphore
		 * without blocking. Oh well.
		 */
		if (unlikely(!ps_cas(&s->rescnt, rescnt, rescnt - 1))) continue; /* retry */

		/* If we don't need to block, return */
		if (likely(rescnt > SYNC_SEM_ZERO)) return;

		sync_blkpt_wait(&s->blkpt, 0, &chkpt);
		/*
		 * When we wakeup, attempt to take the semaphore
		 * again, so this assumes that when waking, the count
		 * is reset to 1 even if it is arbitrarily negative
		 * (assuming we wake all blocked threads in the
		 * blockpoint).
		 */
	}
}

/**
 * Attempts to take the semaphore, and returns a value depending on if it
 * takes it, or not.
 *
 * - @s - the `sync_sem`
 * - @return - `0` on successful semaphore acquisition,
 *             '1' if there are no semaphore resources available.
 */
static inline int
sync_sem_try_take(struct sync_sem *s)
{
	unsigned long rescnt;

	while (1) {
		rescnt = ps_load(&s->rescnt);

		/* If taking the semaphore would block, return as such */
		if (rescnt <= SYNC_SEM_ZERO) return 1;

		/*
		 * Attempt to take a count and return; any changes
		 * deserve a "retry". Note this CAS *can* fail even
		 * when multiple threads are able to take the
		 * semaphore without blocking. Oh well.
		 */
		if (unlikely(!ps_cas(&s->rescnt, rescnt, rescnt - 1))) continue; /* retry */

		/* Successfully taken! */
		return 0;
	}
}

/**
 * Give a semaphore. You may only give one at a time.
 *
 * @precondition: None.
 *
 * - @s - the semaphore
 */
static inline void
sync_sem_give(struct sync_sem *s)
{
	while (1) {
		unsigned long rescnt = ps_load(&s->rescnt);

		if (likely(rescnt >= SYNC_SEM_ZERO)) {
			/* No blocked threads. Just increment the count. */
			if (!ps_cas(&s->rescnt, rescnt, rescnt + 1)) continue; /* retry */
		} else {
			/*
			 * We have blocked threads. Wake them giving
			 * one resource for one of them. The rest will
			 * again take the count below zero, and
			 * reblock. Not great, but this is the
			 * solution until we have the ability to wake
			 * only one thread.
			 */
			if (!ps_cas(&s->rescnt, rescnt, SYNC_SEM_ZERO + 1)) continue; /* retry */
			/* if there are blocked threads, wake 'em up! */
			sync_blkpt_wake(&s->blkpt, 0);
		}

		return;
	}
}

#endif /* SYNC_SEM_H */
