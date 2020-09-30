#ifndef CRT_SEM_H
#define CRT_SEM_H

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
#include <crt_blkpt.h>

#define MAX_SEMS		10000000
#define CONCURRENT_TAKES	1000

struct crt_sem {
	unsigned long rescnt;
	struct crt_blkpt blkpt;
};

/**
 * Initialize a semaphore. Does *not* allocate memory for it, and assumes
 * that you pass that memory in as an argument.
 *
 * - @s - the semaphore
 * - @resnum - the number of the resources
 * - @return - `0` on successful initialization,
 *             `!0` if the backing blockpoint cannot be allocated
 */
static inline int
crt_sem_init(struct crt_sem *s, unsigned long resnum)
{
	if (resnum >= MAX_SEMS) return -1;

	s->rescnt = resnum + CONCURRENT_TAKES;

	return crt_blkpt_init(&s->blkpt);
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
crt_sem_teardown(struct crt_sem *s)
{
	return crt_blkpt_teardown(&s->blkpt);
}

/**
 * Take a semaphore. You may only take one at a time.
 *
 * @precondition - none.
 *
 * - @s - the semaphore
 */
static inline void
crt_sem_take(struct crt_sem *s)
{
	struct crt_blkpt_checkpoint chkpt;

	while (1) {
		crt_blkpt_checkpoint(&s->blkpt, &chkpt);
		if (ps_faa(&s->rescnt, -1) > CONCURRENT_TAKES) {
			return;	/* success! */
		}
		/* failure: try and block */
		ps_faa(&s->rescnt, 1);
		crt_blkpt_wait(&s->blkpt, 0, &chkpt);
	}
}

/**
 * Attempts to take the semaphore, and returns a value depending on if it
 * takes it, or not.
 *
 * - @s - the `crt_sem`
 * - @return - `0` on successful semaphore acquisition,
 *             '1' if there are no semaphores anymore.
 */
static inline int
crt_sem_try_take(struct crt_sem *s)
{
	if (ps_faa(&s->rescnt, -1) > CONCURRENT_TAKES) {
		return 0;	/* success! */
	}
	ps_faa(&s->rescnt, 1);
	return 1;
}

/**
 * Give a semaphore. You may only give one at a time.
 *
 * @precondition: None.
 *
 * - @s - the semaphore
 */
static inline void
crt_sem_give(struct crt_sem *s)
{
	/* If we'd have enough of semaphores, saturate it */
	if (s->rescnt > (CONCURRENT_TAKES + MAX_SEMS)) return;
	
	ps_faa(&s->rescnt, 1);

	/* if there are blocked threads, wake 'em up! */
	crt_blkpt_trigger(&s->blkpt, 0);
}

#endif /* CRT_SEM_H */
