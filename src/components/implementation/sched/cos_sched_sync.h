#ifndef COS_SCHED_SYNC_H
#define COS_SCHED_SYNC_H

#include <consts.h>
#include <cos_types.h>

#include <cos_component.h>
#include <cos_debug.h>

PERCPU_EXTERN(cos_sched_notifications);

/* Following two functions are also needed by the low-level booter to
 * provide scheduler functionality when booting the system. */
/*
 * We cannot just pass the thread id into the system call in registers
 * as the current thread of control making the switch_thread system
 * call might be preempted after deciding based on memory structures
 * which thread to run, but before the actual system call is made.
 * The preempting thread might change the current threads with high
 * priority.  When the system call ends up being executed, it is on
 * stale info, and a thread is switched to that might be actually be
 * interesting.
 *
 * Storing in memory the intended thread to switch to, allows other
 * preempting threads to update the next_thread even if a thread is
 * preempted between logic and calling switch_thread.
 */
static inline int cos_switch_thread(unsigned short int thd_id, unsigned short int flags)
{
	struct cos_sched_next_thd *cos_next = &PERCPU_GET(cos_sched_notifications)->cos_next;

        /* This must be volatile as we must commit what we want to
	 * write to memory immediately to be read by the kernel */
	cos_next->next_thd_id = thd_id;
	cos_next->next_thd_flags = flags;

	/* kernel will read next thread information from cos_next */
	return cos___switch_thread(thd_id, flags); 
}

/*
 * If you want to switch to a thread after an interrupt that is
 * currently executing is finished, that thread can be set here.  This
 * is a common case: An interrupt's execution wishes to wake up a
 * thread, thus it calls the scheduler.  Assume the woken thread is of
 * highest priority besides the interrupt thread.  When the interrupt
 * completes, it should possibly consider switching to that thread
 * instead of the one it interrupted.  This is the mechanism for
 * telling the kernel to look at the thd_id for execution when the
 * interrupt completes.
 */
static inline void cos_next_thread(unsigned short int thd_id)
{
	volatile struct cos_sched_next_thd *cos_next = &PERCPU_GET(cos_sched_notifications)->cos_next;

	cos_next->next_thd_id = thd_id;
}

/* 
 * The lock contains the thread that owns it (or 0 if it is not
 * taken), and the thread id of the most recent thread to contend the
 * lock (if one did).  Here we just do atomic operations to ensure
 * that this is so.
 */

static inline int cos_sched_lock_take(void)
{
	union cos_synchronization_atom *l = &PERCPU_GET(cos_sched_notifications)->cos_locks;
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
	union cos_synchronization_atom *l = &PERCPU_GET(cos_sched_notifications)->cos_locks;
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
	union cos_synchronization_atom *l = &PERCPU_GET(cos_sched_notifications)->cos_locks;
	
	return l->c.owner_thd == cos_get_thd_id();
}

#endif
