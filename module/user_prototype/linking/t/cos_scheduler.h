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

#include <cos_component.h>
#include <cos_debug.h>

/*************** Scheduler Synchronization Fns ***************/

static inline int cos_sched_lock_take(void)
{
	struct cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
	unsigned int curr_thd = cos_get_thd_id();
	
	while (1) {
		int ret;
		unsigned int lock_val;

		__asm__ __volatile__("call cos_atomic_user1"
				     : "=D" (lock_val) 
				     : "a" (l), "b" (curr_thd)
				     : "cc", "memory");
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
	struct cos_synchronization_atom *l = &cos_sched_notifications.cos_locks;
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


/**************** Scheduler Util Fns *******************/

#define THD_BLOCKED 0x1
#define THD_READY   0x2
#define THD_FREE    0x4
#define THD_GRP     0x8  // is this thread a group of thds?
#define THD_MEMBER  0x10 // is this thread part of a group?
#define THD_UC_ACTIVE 0X20
#define THD_UC_READY  0X40

#define sched_thd_free(thd)    ((thd)->flags & THD_FREE)
#define sched_thd_grp(thd)     ((thd)->flags & THD_GRP)
#define sched_thd_member(thd)  ((thd)->flags & THD_MEMBER)
#define sched_thd_ready(thd)   ((thd)->flags & THD_READY)
#define sched_thd_blocked(thd) ((thd)->flags & THD_BLOCKED)

#define SCHED_NUM_THREADS MAX_NUM_THREADS

#define INIT_LIST(obj, next, prev)		  \
	(obj)->next = (obj)->prev = (obj)

#define ADD_LIST(head, new, next, prev) 	  \
	(new)->next = (head)->next;		  \
	(new)->prev = (head);			  \
	(head)->next = (new);			  \
	(new)->next->prev = (new)

#define REM_LIST(obj, next, prev)                 \
	(obj)->next->prev = (obj)->prev;	  \
	(obj)->prev->next = (obj)->next;	  \
	(obj)->next = (obj)->prev = (obj)

#define FIRST_LIST(obj, next, prev)               \
	((obj)->next)

#define LAST_LIST(obj, next, prev)                \
	((obj)->prev)

#define EMPTY_LIST(obj, next, prev)		  \
	((obj)->next == (obj))

struct sched_accounting {
	unsigned long C, T, C_used, T_left;
	unsigned long long cycles;
};

struct sched_metric {
	unsigned short int priority, urgency;
};

struct sched_thd {
	unsigned short int flags, id, evt_id;
	struct sched_accounting accounting;
	struct sched_metric metric;
	struct sched_thd *prio_next, *prio_prev;
	int wake_cnt;

	/* If flags & THD_MEMBER */
	struct sched_thd *group;
	/* If flags & THD_GRP */
	int nthds;

	/* linked list for all threads in a group */
	struct sched_thd *next, *prev;
};

void sched_init_thd(struct sched_thd *thd, unsigned short int id, int flags);
struct sched_thd *sched_alloc_thd(unsigned short int id);
struct sched_thd *sched_alloc_upcall_thd(unsigned short int thd_id);
void sched_ds_init(void);
void sched_free_thd(struct sched_thd *thd);
void sched_make_grp(struct sched_thd *thd, unsigned short int sched_thd);
void sched_add_grp(struct sched_thd *grp, struct sched_thd *thd);
void sched_rem_grp(struct sched_thd *grp, struct sched_thd *thd);

static inline struct sched_accounting *sched_get_accounting(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	return &thd->accounting;
}

static inline struct sched_metric *sched_get_metric(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	return &thd->metric;
}

/**************** Scheduler Event Fns *******************/

typedef void (*sched_evt_visitor_t)(struct sched_thd *t, u8_t flags, u32_t cpu_consumption);
int cos_sched_process_events(sched_evt_visitor_t fn, unsigned int proc_amnt);
void cos_sched_set_evt_urgency(u8_t id, u16_t urgency);
short int sched_alloc_event(struct sched_thd *thd);
extern struct sched_thd *sched_map_evt_thd[NUM_SCHED_EVTS];
static inline struct sched_thd *sched_evt_to_thd(short int evt_id)
{
	assert(evt_id < NUM_SCHED_EVTS);
	assert(evt_id != 0);

	return sched_map_evt_thd[evt_id];
}
static inline void sched_set_thd_urgency(struct sched_thd *t, u16_t urgency)
{
	if (t->evt_id) {
		cos_sched_set_evt_urgency(t->evt_id, urgency);
	}
	sched_get_metric(t)->urgency = urgency;
}



/* --- Thread Id -> Sched Thread Mapping Utilities --- */

/* 
 * FIXME: add locking.
 */

extern struct sched_thd *sched_thd_map[];
static inline struct sched_thd *sched_get_mapping(unsigned short int thd_id)
{
	if (thd_id >= SCHED_NUM_THREADS ||
	    sched_thd_map[thd_id] == NULL ||
	    (sched_thd_map[thd_id]->flags & THD_FREE)) {
		return NULL;
	}

	return sched_thd_map[thd_id];
}

static inline struct sched_thd *sched_get_current(void)
{
	unsigned short int thd_id;
	struct sched_thd *thd;

	thd_id = cos_get_thd_id();
	thd = sched_get_mapping(thd_id);
	
	return thd;
}

static inline int sched_add_mapping(unsigned short int thd_id, struct sched_thd *thd)
{
	if (thd_id >= SCHED_NUM_THREADS ||
	    sched_thd_map[thd_id] != NULL) {
		return -1;
	}
	
	sched_thd_map[thd_id] = thd;

	return 0;
}

static inline void sched_rem_mapping(unsigned short int thd_id)
{
	if (thd_id >= SCHED_NUM_THREADS) return;

	sched_thd_map[thd_id] = NULL;
}

static inline int sched_is_grp(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	if (thd->flags & THD_GRP) {
		assert(!sched_thd_member(thd));
		
		return 1;
	}
	assert(!sched_thd_grp(thd));

	return 0;
}

static inline struct sched_thd *sched_get_members(struct sched_thd *grp)
{
	assert(!sched_thd_free(grp) && sched_thd_grp(grp));

	if (grp->next == grp) return NULL;
	return grp->next;
}

static inline struct sched_thd *sched_get_grp(struct sched_thd *thd)
{
	if (sched_is_grp(thd)) {
		return NULL;
	}
	return thd->group;
}

#endif
