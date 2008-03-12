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
#include "cos_list.h"

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

#endif
