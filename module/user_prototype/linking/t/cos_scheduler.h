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

#define COS_THD_BLOCKED 0x1
#define COS_THD_READY   0x2
#define COS_THD_FREE    0x4

struct sched_accounting {
	unsigned long quantum, res_used;
};

struct cos_thread;
struct thread_group {
	unsigned short int spd_id, status;
	struct sched_accounting accounting;
	struct cos_thread *first, *last, *threads;
	struct thread_group *next;
	int num_thds;
};

struct thd_grp_q {
	struct thread_group *first, *last;
};

struct cos_thread {
	unsigned short int thd_id, priority, urgency, status;
	struct sched_accounting accounting;
	struct cos_thread *next;
	struct thread_group *grp;
};

static inline void init_thd_grp(struct thread_group *grp, unsigned short int spd_id)
{
	grp->spd_id = spd_id;
	grp->first = grp->last = grp->threads
}

static inline void init_thread_array(struct cos_thread *thds, unsigned int num) {
	int i;

	for (i = 0 ; i < num ; i++) {
		thds[i]->status = COS_THD_FREE;
	}

	return;
}

static inline struct cos_thread *find_free_thd(struct cos_thread *thds, unsigned int num)
{
	int i;

	for (i = 0 ; i < num ; i++) {
		struct cos_thread *thd = &thds[i];

		if (thd->status & COS_THD_FREE) {
			thd->status &= ~COS_THD_FREE;
			return thd;
		}
	}
	
	return NULL;
}

static inline void activate_thd_in_grp(struct thread_group *thd_grp, struct cos_thread *thd)
{
	struct cos_thread *last_thd;

	last_thd = thd_grp->last;
	if (!last_thd) 
		last_thd->next = thd;
	else
		thd_grp->first = thd;

	thd_grp->last = thd;
	thd->next = NULL;
	
	return;
}

#endif
