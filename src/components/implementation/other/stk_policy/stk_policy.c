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
#include <stkmgr.h>

#include <cos_list.h>
#include <heap.h>

#define POLICY_PERIODICITY 50

/* data-structures */

struct thd_sched {
	int period, misses, priority;
	long lateness, miss_lateness;
};

struct component {
	spdid_t spdid;
	unsigned int concur_prev, concur_est, concur_new;
	struct component *next, *prev;
};

struct thd_comp {
	unsigned long time_blocked, tot_cost;
	int stack_misses;
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
int ncomps = 0;
struct heap *h;

static struct component *
find_component(spdid_t spdid)
{
	struct component *c;

	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ; 
	     c = FIRST_LIST(c, next, prev)) {
		if (spdid == c->spdid) return c;
	}
	return NULL;
}

static void 
gather_data(void)
{
	struct thd *titer;
	struct component *citer;

	for (citer = FIRST_LIST(&components, next, prev) ; 
	     citer != &components ; 
	     citer = FIRST_LIST(citer, next, prev)) {
		int est;
		
		est = stkmgr_spd_concurrency_estimate(citer->spdid);
		assert(est != -1);
		citer->concur_prev = citer->concur_est;
		citer->concur_est = est;
	}

	for (titer = FIRST_LIST(&threads, next, prev) ; 
	     titer != &threads ; 
	     titer = FIRST_LIST(titer, next, prev)) {
		unsigned short int tid = titer->tid;
		int i;

		/* Scheduling info */
		titer->sched_info.misses = periodic_wake_get_misses(tid);
		titer->sched_info.lateness = periodic_wake_get_lateness(tid);
		titer->sched_info.miss_lateness = periodic_wake_get_miss_lateness(tid);

		/* Component stack info */
		for (i = 0 ; i < ncomps ; i++) {
			struct thd_comp *tc;

			tc = &titer->comp_info[i];
			assert(tc && tc->c);
			tc->time_blocked = stkmgr_thd_blk_time(tid, tc->c->spdid, 0);
			tc->stack_misses = stkmgr_thd_blk_cnt(tid, tc->c->spdid, 1);
			assert(tc->time_blocked != (unsigned long)-1 && tc->stack_misses >= 0);
		}
	}
}

static void
process_data(void)
{
	
}

static void
policy(void)
{
	struct thd *iter;

	for (iter = FIRST_LIST(&threads, next, prev) ; 
	     iter != &threads ; 
	     iter = FIRST_LIST(iter, next, prev)) {
		struct thd_sched *si = &iter->sched_info;

		printc("Thread %d, per %d, prio %d: %d misses, %ld lateness, %ld miss lateness.\n", 
		       iter->tid, si->period, si->priority, si->misses, si->lateness, si->miss_lateness);
	}
}

static struct thd *
create_thread(void)
{
	struct thd *t = malloc(sizeof(struct thd));

	if (!t) BUG();
	memset(t, 0, sizeof(struct thd));

	return t;
}

/* insertion sort...only do once */
static int 
insert_thread(struct thd *t)
{
	struct thd *iter;

	for (iter = FIRST_LIST(&threads, next, prev) ; 
	     iter->sched_info.priority < t->sched_info.priority && iter != &threads ; 
	     iter = FIRST_LIST(iter, next, prev));

	ADD_LIST(LAST_LIST(iter, next, prev), t, next, prev);

	return 0;
}

static void 
init_thds(void)
{
	unsigned short int i, j;
	
	/* initialize the spds first! */
	assert(ncomps);
	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		int p;
		struct thd *t;
		struct component *c;
		
		p = periodic_wake_get_period(i);
		if (0 >= p) continue;

		t = create_thread();
		t->tid = i;
		t->sched_info.period = p;
		p = sched_priority(i);
		t->sched_info.priority = p;
		insert_thread(t);

		c = FIRST_LIST(&components, next, prev);
		for (j = 0 ; j < ncomps ; j++) {
			assert(&components != c);
			t->comp_info[j].c = c;
			c = FIRST_LIST(c, next, prev);
		}
	}
}

static void
init_spds(void)
{
	int i;
	
	INIT_LIST(&components, next, prev);
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		struct component *c;

		if (-1 == stkmgr_spd_concurrency_estimate(i)) continue;
		c = malloc(sizeof(struct component));
		if (!c) BUG();
		memset(c, 0, sizeof(struct component));
		c->spdid = i;
		INIT_LIST(c, next, prev);
		ADD_LIST(&components, c, next, prev);
		ncomps++;
	}
}

void 
cos_init(void *arg)
{
	int c = 0;

	INIT_LIST(&threads, next, prev);
	/* Wait for all other threads to initialize */
	timed_event_block(cos_spd_id(), 97);

	init_spds();
	init_thds();
	periodic_wake_create(cos_spd_id(), POLICY_PERIODICITY);
	while (1) {
		gather_data();
		policy();
		if (c == 5) c = 0;
		else c = 5;
		stkmgr_stack_report();
		stkmgr_set_concurrency(14, c);
		stkmgr_stack_report();
		periodic_wake_wait(cos_spd_id());
	}
	return;
}

void 
bin (void)
{
	sched_block(cos_spd_id(), 0);
}
