#ifndef COS_SCHED_SYNC_H
#define COS_SCHED_SYNC_H

#include <consts.h>
#include <cos_types.h>

#include <cos_component.h>
#include <cos_debug.h>

static inline int cos_sched_lock_take(void)
{
	struct cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	unsigned int curr_thd = cos_get_thd_id();
	
	/* Recursively taking the lock: not good */
	assert(l->owner_thd != curr_thd);
	while (1) {
		unsigned int lock_val;

		__asm__ __volatile__("call cos_atomic_user1"
				     : "=D" (lock_val) 
				     : "a" (l), "b" (curr_thd)
				     : "cc", "memory");
		/* no contention?  We're done! */
		if (lock_val == 0) {
			break;
		}
		/* If another thread holds the lock, notify kernel to switch */
		if (cos___switch_thread(lock_val & 0x0000FFFF, COS_SCHED_SYNC_BLOCK) == -1) {
			return -1;
		}
	} 

	return 0;
}

static inline int cos_sched_lock_release(void)
{
	struct cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	unsigned int lock_val;
	/* TODO: sanity check that verify that lower 16 bits of
	   lock_val == curr_thd unsigned int curr_thd =
	   cos_get_thd_id(); */
	assert(l->owner_thd == cos_get_thd_id());
	__asm__ __volatile__("call cos_atomic_user2"
			     : "=c" (lock_val)
			     : "a" (l)
			     : "memory");
	/* If a thread is attempting to access the resource, */
	lock_val >>= 16;
	if (lock_val) {
		//assert(sched_get_current()->id != lock_val);
		return cos___switch_thread(lock_val, COS_SCHED_SYNC_UNBLOCK);
	}
	
	return 0;

}

/* do we own the lock? */
static inline int cos_sched_lock_own(void)
{
	struct cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	
	return l->owner_thd == cos_get_thd_id();
}

#endif
