/*
 * Scheduler library that can be used by schedulers to manage their
 * data structures.
 *
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
#ifndef COS_SCHEDULER_H
#define COS_SCHEDULER_H

#include "../../../include/consts.h"
#include "../../../include/cos_types.h"

#include "cos_component.h"

static inline int cos_sched_lock_take(void)
{
	struct cos_synchronization_atom *l = &cos_sched_notifications.locks;
	unsigned int curr_thd = cos_get_thd_id();
	
	while (1) {
		int ret;
		unsigned int lock_val;

		__asm__ __volatile__("call cos_atomic_user1"
				     : "=D" (lock_val) 
				     : "a" (l), "b" (curr_thd)
				     : "cc", "memory");
//		print_vals(0, curr_thd, (lock_val & 0xFFFF0000) >> 16, (0x0000FFFF & lock_val));
		/* no contention?  We're done! */
		if (lock_val == 0) {
			break;
		}
		/* If another thread holds the lock, notify lock component */
		if ((ret = cos___switch_thread(lock_val & 0x0000FFFF, COS_SCHED_SYNC_BLOCK)) == -1) {
			return -1;
		}
	} 

	return 0;
}

static inline int cos_sched_lock_release(void)
{
	struct cos_synchronization_atom *l = &cos_sched_notifications.locks;
	unsigned int lock_val;
	/* TODO: sanity check that verify that lower 16 bits of
	   lock_val == curr_thd unsigned int curr_thd =
	   cos_get_thd_id(); */
	
	__asm__ __volatile__("call cos_atomic_user2"
			     : "=c" (lock_val)
			     : "a" (l)
			     : "memory");
	/* If a thread is attempting to access the resource, */
	lock_val >>= 16;
	if (lock_val) {
		return cos___switch_thread(lock_val, COS_SCHED_SYNC_UNBLOCK);
	}
	
	return 0;

}

/*
 * This will call the switch_thread syscall after releasing the
 * scheduler lock.
 */
static inline int cos_switch_thread_release(unsigned short int thd_id, 
					    unsigned short int flags, 
					    unsigned int urgency)
{
        /* This must be volatile as we must commit what we want to
	 * write to memory immediately to be read by the kernel */
	volatile struct cos_sched_next_thd *cos_next = &cos_sched_notifications.cos_next;

	cos_next->next_thd_id = thd_id;
	cos_next->next_thd_flags = flags;
	cos_next->next_thd_urgency = urgency;

	cos_sched_lock_release();

	/* kernel will read next thread information from cos_next */
	return cos___switch_thread(thd_id, flags); 
}

#ifdef WAIT

#define THD_BLOCKED 0x1
#define THD_READY   0x2
#define THD_FREE    0x4
#define THD_GRP     0x8  // is this thread a group of thds?
#define THD_MEMBER  0x10 // is this thread part of a group?

struct sched_accounting {
	unsigned long C, T, C_used, T_left;
};

struct sched_metric {
	unsigned short int priority, urgency;
};

struct sched_thd {
	unsigned short int flags;
	unsigned short int thd_id;
	struct sched_accounting accounting;
	struct sched_metric metric;
	list_ptr_t list;
	
	/* If flags & THD_MEMBER */
	struct sched_thd *group;

	/* If flags & THD_GRP */
	struct sched_thd *threads;
	int nthds;
};

void sched_init_thd(struct sched_thd *thd, unsigned short int id, 
		    unsigned short int sched_thd);
void sched_init_thd_array(void); 
struct sched_thd *sched_alloc_thd(void);

LIST_OPS_CREATE(sched_thd,,list)

#define SCHED_NUM_THREADS MAX_NUM_THREADS
/* * 2 for thread groups */
#define SCHED_NUM_EXECUTABLES (SCHED_NUM_THREADS * 2) 

static inline struct sched_accounting *sched_get_accounting(struct sched_thd *thd)
{
	assert(thd->flags & THD_FREE == 0);

	return &thd->accounting;
}

static inline struct sched_metric *sched_get_metric(struct sched_thd *thd)
{
	assert(thd->flags & THD_FREE == 0);

	return &thd->metric;
}

/* --- Thread Mapping Utilities --- */

extern struct sched_thread **thd_map;
static inline struct sched_thd *sched_get_mapping(unsigned short int thd_id)
{
	if (thd_id >= SCHED_NUM_THREADS ||
	    thd_map[thd_id] == NULL ||
	    thd_map[thd_id].flags & THD_FREE) {
		return NULL;
	}

	return thd_map[thd_id];
}

static inline int sched_add_mapping(unsigned short int thd_id, struct sched_thd *thd)
{
	if (thd_id >= SCHED_NUM_THREADS ||
	    thd_map[thd_id] != NULL) {
		return -1;
	}
	
	thd_map[thd_id] = thd;

	return 0;
}

static inline void sched_rem_mapping(unsigned short int thd_id)
{
	if (thd_id >= SCHED_NUM_THREADS) return;

	thd_map[thd_id] = NULL;
}
#endif /* WAIT */

#endif
