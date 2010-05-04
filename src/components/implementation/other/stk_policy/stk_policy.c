/**
 * Copyright 2010 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>

#include <periodic_wake.h>
#include <sched.h>
#include <timed_blk.h>
#include <sched_conf.h>
#include <cos_alloc.h>

#include <cos_list.h>
#include <heap.h>

#define POLICY_PERIODICITY 50

/* data-structures */

struct thd_sched {
	int period, misses, priority;
	long lateness, miss_lateness;
};

struct component {
	unsigned int allocated, concur_estimate;
	unsigned int new_alloc;
};

struct thd_comp {
	unsigned long time_blocked;
	unsigned int stack_misses;
	struct component *c;
};

struct thd {
	unsigned short int tid;
	struct thd_sched sched_info;
	struct thd_comp comp_info[MAX_NUM_SPDS];

	struct thd *next, *prev;
};

struct thd threads;
struct component components;
struct heap *h;

static void gather_information(void)
{
	struct thd *iter;

	for (iter = FIRST_LIST(&threads, next, prev) ; 
	     iter != &threads ; 
	     iter = FIRST_LIST(iter, next, prev)) {
		unsigned short int tid = iter->tid;

		iter->sched_info.misses = periodic_wake_get_misses(tid);
		iter->sched_info.lateness = periodic_wake_get_lateness(tid);
		iter->sched_info.miss_lateness = periodic_wake_get_miss_lateness(tid);
	}
}

static void policy(void)
{
	struct thd *iter;

	gather_information();

	for (iter = FIRST_LIST(&threads, next, prev) ; 
	     iter != &threads ; 
	     iter = FIRST_LIST(iter, next, prev)) {
		struct thd_sched *si = &iter->sched_info;

		printc("Thread %d, per %d, prio %d: %d misses, %ld lateness, %ld miss lateness.\n", 
		       iter->tid, si->period, si->priority, si->misses, si->lateness, si->miss_lateness);
	}
}

static struct thd *create_thread(void)
{
	struct thd *t = malloc(sizeof(struct thd));

	if (!t) BUG();
	memset(t, 0, sizeof(struct thd));

	return t;
}

/* insertion sort...only do once */
static int insert_thread(struct thd *t)
{
	struct thd *iter;

	for (iter = FIRST_LIST(&threads, next, prev) ; 
	     iter->sched_info.priority < t->sched_info.priority && iter != &threads ; 
	     iter = FIRST_LIST(iter, next, prev));

	ADD_LIST(LAST_LIST(iter, next, prev), t, next, prev);

	return 0;
}

static void init_thds(void)
{
	unsigned short int i;

	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		int p;
		struct thd *t;
		
		p = periodic_wake_get_period(i);
		if (0 >= p) continue;

		t = create_thread();
		t->tid = i;
		t->sched_info.period = p;
		p = sched_priority(i);
		t->sched_info.priority = p;
		insert_thread(t);
	}
}

void cos_init(void *arg)
{
	INIT_LIST(&threads, next, prev);
	/* Wait for all other threads to initialize */
	timed_event_block(cos_spd_id(), 97);

	init_thds();
	periodic_wake_create(cos_spd_id(), POLICY_PERIODICITY);
	while (1) {
		policy();
		periodic_wake_wait(cos_spd_id());
	}
	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}
