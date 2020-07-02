#ifndef CRT_LOCK_H
#define CRT_LOCK_H

/***
 * Simple blocking lock. Uses blockpoints to enable the blocking and
 * waking of contending threads. This has little to no intelligence,
 * for example, not expressing dependencies for PI.
 */

#include <cos_component.h>
#include <crt_blkpt.h>

struct crt_lock {
	unsigned long owner;
	struct crt_blkpt blkpt;
};

static inline int
crt_lock_init(struct crt_lock *l)
{
	l->owner = 0;

	return crt_blkpt_init(&l->blkpt);
}

static inline int
crt_lock_teardown(struct crt_lock *l)
{
	assert(l->owner == 0);

	return crt_blkpt_teardown(&l->blkpt);
}

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

static inline void
crt_lock_release(struct crt_lock *l)
{
	assert(l->owner == cos_thdid());
	l->owner = 0;
	/* if there are blocked threads, wake 'em up! */
	crt_blkpt_trigger(&l->blkpt, 0);
}

#endif /* CRT_LOCK_H */
