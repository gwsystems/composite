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

#define MAX_SEMS		10000000
#define CONCURRENT_TAKES	1000

struct sync_sem {
	unsigned long rescnt_blked;
	struct sync_blkpt blkpt;
};

#define SYNC_SEM_OWNER_BLKED_BITS  (sizeof(unsigned long) * 8)
#define SYNC_SEM_BLKED_MASK        (1UL << (SYNC_SEM_OWNER_BLKED_BITS - 2))
#define SYNC_SEM_BLKED(e)          ((e) &  SYNC_SEM_BLKED_MASK)
#define SYNC_SEM_RESCNT(e)         ((e) & ~SYNC_SEM_BLKED_MASK)

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
sync_sem_init(struct sync_sem *s, unsigned long resnum)
{
	if (resnum >= MAX_SEMS) return -1;

	s->rescnt_blked = resnum + CONCURRENT_TAKES;

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
	unsigned long cached;

	cached = ps_load(&s->rescnt_blked);
	assert(SYNC_SEM_BLKED(s->rescnt_blked) == 0);
	if (!ps_cas(&s->rescnt_blked, cached, ~0)) return 1;

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
		unsigned long rescnt_blked;

		sync_blkpt_checkpoint(&s->blkpt, &chkpt);

		/* Can we take the semaphore? If the number of semaphores are not zero, we try to take it */
		if (SYNC_SEM_RESCNT(ps_faa(&s->rescnt_blked, -1)) > CONCURRENT_TAKES) {
			return;	/* success! */
		}

		/* slowpath: we're blocking! Set the blocked bit, or try again */
		ps_faa(&s->rescnt_blked, 1);
		rescnt_blked = ps_load(&s->rescnt_blked);
		if (!ps_cas(&s->rescnt_blked, rescnt_blked, rescnt_blked | SYNC_SEM_BLKED_MASK)) continue;

		/* We can't take the semaphore, have set the block bit, and await release */
		sync_blkpt_wait(&s->blkpt, 0, &chkpt);
	}
}

/**
 * Attempts to take the semaphore, and returns a value depending on if it
 * takes it, or not.
 *
 * - @s - the `sync_sem`
 * - @return - `0` on successful semaphore acquisition,
 *             '1' if there are no semaphores anymore.
 */
static inline int
sync_sem_try_take(struct sync_sem *s)
{
	if (SYNC_SEM_RESCNT(ps_faa(&s->rescnt_blked, -1)) > CONCURRENT_TAKES) {
		return 0;	/* success! */
	}
	ps_faa(&s->rescnt_blked, 1);
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
sync_sem_give(struct sync_sem *s)
{
	while (1) {
		unsigned long o_b = ps_load(&s->rescnt_blked);
		int blked = unlikely(SYNC_SEM_BLKED(o_b) == SYNC_SEM_BLKED_MASK);
		int rescnt = SYNC_SEM_RESCNT(o_b);

		/* If we'd have enough number of semaphores, saturate it */
		if (rescnt < (CONCURRENT_TAKES + MAX_SEMS)) rescnt++;

		/* Give semaphore back and clear the blocked bit, with CAS */
		if (unlikely(!ps_cas(&s->rescnt_blked, o_b, rescnt))) continue;

		/* if there are blocked threads, wake 'em up! */
		if (unlikely(blked)) sync_blkpt_wake(&s->blkpt, 0);

		return;
	}
}

#endif /* SYNC_SEM_H */
