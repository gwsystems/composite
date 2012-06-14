#ifndef COS_SCHED_SYNC_H
#define COS_SCHED_SYNC_H

#include <consts.h>
#include <cos_types.h>

#include <cos_component.h>
#include <cos_debug.h>

/* 
 * The lock contains the thread that owns it (or 0 if it is not
 * taken), and the thread id of the most recent thread to contend the
 * lock (if one did).  Here we just do atomic operations to ensure
 * that this is so.
 */

static inline int cos_sched_lock_take(void)
{
	union cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	u16_t curr_thd = cos_get_thd_id(), owner;
	
	/* Recursively taking the lock: not good */
	assert(l->c.owner_thd != curr_thd);
	do {
		union cos_synchronization_atom p, n; /* previous and new */

		do {
			p.v            = l->v;
			owner          = p.c.owner_thd;
			n.c.queued_thd = 0; /* will be set in the kernel... */
			if (unlikely(owner)) n.c.owner_thd = owner;
			else                 n.c.owner_thd = curr_thd;
		} while (unlikely(!cos_cas((unsigned long *)&l->v, p.v, n.v)));

		if (unlikely(owner)) {
			/* If another thread holds the lock, notify
			 * kernel to switch */
			if (cos___switch_thread(owner, COS_SCHED_SYNC_BLOCK) == -1) return -1;
		}
		/* If we are now the owner, we're done.  If not, try
		 * to take the lock again! */
	} while (unlikely(owner));

	return 0;
}

static inline int cos_sched_lock_release(void)
{
	union cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	union cos_synchronization_atom p;
	u16_t queued_thd;

	assert(l->c.owner_thd == cos_get_thd_id());
	do {
		p.v           = l->v;
		queued_thd    = p.c.queued_thd;
	} while (unlikely(!cos_cas((unsigned long *)&l->v, p.v, 0)));
	/* If a thread is contending the lock. */
	if (queued_thd) return cos___switch_thread(queued_thd, COS_SCHED_SYNC_UNBLOCK);
	
	return 0;

}

/* do we own the lock? */
static inline int cos_sched_lock_own(void)
{
	union cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	
	return l->c.owner_thd == cos_get_thd_id();
}

#endif
