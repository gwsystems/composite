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
#define DEFAULT_STACK_AMNT 5

#include <cos_list.h>
#include <heap.h>

#define POLICY_PERIODICITY 100

/* data-structures */

struct thd_sched {
	int period, priority;

	/* These change over time */
	int misses, deadlines;
	long lateness, miss_lateness;
};

struct component {
	spdid_t spdid;
	unsigned int allocated, concur_est, concur_new;
	struct component *next, *prev;
};

struct thd_comp {
	unsigned long avg_time_blocked, tot_time_blocked, time_per_deadline;
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
		//citer->concur_prev = citer->concur_est;
		citer->concur_est = est;
		printc("Spd %d concurrency estimate: %d\n", citer->spdid, est);
	}

	for (titer = FIRST_LIST(&threads, next, prev) ; 
	     titer != &threads ; 
	     titer = FIRST_LIST(titer, next, prev)) {
		unsigned short int tid = titer->tid;
		struct thd_sched *ts = &titer->sched_info;
		int i;

		/* Scheduling info */
		ts->misses = periodic_wake_get_misses(tid);
		ts->deadlines = periodic_wake_get_deadlines(tid);
		ts->lateness = periodic_wake_get_lateness(tid);
		ts->miss_lateness = periodic_wake_get_miss_lateness(tid);

		printc("Thread %d, period %d, prio %d: %d deadlines, %d misses,"
		       "%ld lateness, %ld miss lateness.\n", 
		       tid, ts->period, ts->priority, ts->deadlines, 
		       ts->misses, ts->lateness, ts->miss_lateness);

		/* Component stack info */
		for (i = 0 ; i < ncomps ; i++) {
			struct thd_comp *tc;

			tc = &titer->comp_info[i];
			assert(tc && tc->c);
			tc->stack_misses = stkmgr_thd_blk_cnt(tid, tc->c->spdid, 0);
			tc->avg_time_blocked = stkmgr_thd_blk_time(tid, tc->c->spdid, 1);
			tc->tot_time_blocked = tc->avg_time_blocked * tc->stack_misses;
			assert(tc->avg_time_blocked != (unsigned long)-1 && tc->stack_misses >= 0);

			if (tc->stack_misses) {
				printc("\tStack info for %d: time blocked %ld, misses %d\n", 
				       tc->c->spdid, tc->avg_time_blocked/1000000, tc->stack_misses);
			}
		}
	}
}

/* 
 * This seems completely broken.  I should be estimating based on past
 * experience, the amount of time that we will be made late due to
 * adding or removing a stack.
 */
static unsigned long
compute_lateness_chg_concur(struct thd *t, struct thd_comp *tc, unsigned int new_concurrency)
{
	long lateness = t->sched_info.lateness;
	int deadlines = t->sched_info.deadlines;
	unsigned long time_blocked = tc->tot_time_blocked;
	unsigned long time_per_deadline, time_per_deadline_stack;
	unsigned int est_concur = tc->c->concur_est, allocated = tc->c->allocated;
	int num_blocked = est_concur - allocated; /* negative means surplus stacks! */
	int change = new_concurrency - allocated;

	if (0 == deadlines)  return 0; /* who knows, we don't have enough info */
	if (change == 0) return lateness;
	
	time_per_deadline = time_blocked/deadlines;

	/* Estimate the effect on block time of a single stack */
	if (num_blocked == 0) {	/* nothing's changed, can't guess */
		time_per_deadline_stack = 0;
	} 
	/* Not enough stacks! */
	else if (num_blocked > 0) {
		/* We assume that each stack allocated/taken away from
		 * this component will effect the lateness by an
		 * amount proportional to the amount of time we
		 * currently spend blocking, and the number of threads
		 * blocking at any point in time. */
		time_per_deadline_stack = time_per_deadline/num_blocked;
	} 
	/* More than enough stacks */
	else if (num_blocked < 0) {
		int spare_stacks = -num_blocked;
		int taken_away = -change;

		if (taken_away <= spare_stacks) { /* no change! */
			time_per_deadline_stack = 0;
		} else {
			time_per_deadline_stack = time_per_deadline/num_blocked;
		}
	}

	return lateness - (time_per_deadline_stack * change);
}

static void
policy(void)
{
	struct thd *iter;
	struct component *c;

/* 	for (iter = FIRST_LIST(&threads, next, prev) ;  */
/* 	     iter != &threads ;  */
/* 	     iter = FIRST_LIST(iter, next, prev)) { */
/* 		//struct thd_sched *si = &iter->sched_info; */
/* 		int i; */

/* 		for (i = 0 ; i < ncomps ; i++) { */
/* 			struct thd_comp *tc; */

/* 			tc = &iter->comp_info[i]; */
/* 			assert(tc && tc->c); */
/* 		} */
/* 	} */

	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		//c->concur_new = c->allocated - 1 > c->concur_est ? 
		//	c->allocated - 1 : c->concur_est;
		c->concur_new = c->concur_est;
		c->concur_new = c->concur_new > 0 ? c->concur_new : 1;
//		stkmgr_set_concurrency(c->spdid, c->concur_new, 1);
		c->allocated = c->concur_new;
	}
}

static void
init_policy(void)
{
	int i;

	for (i = 0 ; i < ncomps ; i++) {
		switch (i) {
		case 9:  stkmgr_set_concurrency(i, 10, 1); break;
		default: stkmgr_set_concurrency(i, 1, 1);
		}
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

		printc("Found thread %d.\n", i);

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
		c->allocated = DEFAULT_STACK_AMNT;
		INIT_LIST(c, next, prev);
		ADD_LIST(&components, c, next, prev);
		ncomps++;
	}
}

void 
cos_init(void *arg)
{
	INIT_LIST(&threads, next, prev);
	/* Wait for all other threads to initialize */
	timed_event_block(cos_spd_id(), 97);
	periodic_wake_create(cos_spd_id(), POLICY_PERIODICITY);

	init_spds();
	init_thds();
	init_policy();
	while (1) {
		gather_data();
		policy();
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
