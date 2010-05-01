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
#include <sched_conf.h>
#include <cos_alloc.h>

#include <cos_list.h>

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
	struct thd_sched sched_info;
	struct thd_comp comp_info[MAX_NUM_SPDS];

	struct thd *next, *prev;
};

struct thd threads;
struct component components;
struct heap *h;

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
	struct thd *i;

	for (i = FIRST_LIST(&threads, next, prev) ; 
	     i->sched_info.priority < t->sched_info.priority && i != &threads ; 
	     i = FIRST_LIST(i, next, prev));

	ADD_LIST(LAST_LIST(i, next, prev), t, next, prev);

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
		t->sched_info.period = p;
		p = sched_priority(i);
		t->sched_info.priority = p;
		insert_thread(t);
	}
}

static void policy(void)
{
	init_thds();
}

void cos_init(void *arg)
{
	INIT_LIST(&threads, next, prev);
	periodic_wake_create(cos_spd_id(), 25);
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
