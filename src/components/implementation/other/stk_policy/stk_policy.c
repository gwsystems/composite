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

/* ALGORITHM: 1 for minimize AVG tardiness, otherwise minimize MAX tardiness*/
#define ALGORITHM 1
 
#define THD_POOL 10

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
	long add_impact, remove_impact;
	int add_in;
	struct component *next, *prev;
};

struct thd_comp {
	unsigned long avg_time_blocked, tot_time_blocked, time_per_deadline;
	/*impact and history impact for one stack*/
	unsigned long impact, old_impact;
	int stack_misses;
	struct component *c;
};

struct thd {
	unsigned short int tid;
	struct thd_sched sched_info;
	struct thd_comp comp_info[MAX_NUM_SPDS];
	long tardiness;
	struct thd *next, *prev;
};

struct thd threads;
struct component components;
int ncomps = 0;
struct heap *h;
int available = MAX_NUM_STACKS;
long largest_tardiness;

static void 
gather_data()
{
	struct thd *titer;
	struct component *citer;

	for (titer = FIRST_LIST(&threads, next, prev) ; 
	     titer != &threads ; 
	     titer = FIRST_LIST(titer, next, prev)) {
		unsigned short int tid = titer->tid;
		struct thd_sched *ts = &titer->sched_info;
		/* Scheduling info */

		ts->misses = periodic_wake_get_misses(tid);
		ts->deadlines = periodic_wake_get_deadlines(tid);
		ts->lateness = periodic_wake_get_lateness(tid);
		ts->miss_lateness = periodic_wake_get_miss_lateness(tid);
		titer->tardiness = ts->miss_lateness > 0 ? ts->miss_lateness : ts->lateness;
		/* printc("Thread %d, period %d, prio %d: %d deadlines, %d misses,"
		       "%ld lateness, %ld miss lateness.\n", 
		       tid, ts->period, ts->priority, ts->deadlines, 
		       ts->misses, ts->lateness, ts->miss_lateness); */
		printc("Thread DLM%d, %ld miss\n", ts->misses, ts->miss_lateness);
		/* } else {
		   ts->misses += periodic_wake_get_misses(tid) + 1;
		   ts->deadlines += periodic_wake_get_deadlines(tid) + 1;
		   ts->lateness += periodic_wake_get_lateness(tid) + 1;
		   ts->miss_lateness += periodic_wake_get_miss_lateness(tid) + 1;
		   ts->misses >>= 1;
		   ts->deadlines >>= 1;
		   ts->lateness >>= 1;
		   ts->miss_lateness >>= 1;
		   titer->tardiness = ts->miss_lateness > 0 ? ts->miss_lateness : ts->lateness;
		   }*/
		/* Component stack info */
		for (citer = FIRST_LIST(&components, next, prev) ; 
		     citer != &components ; 
		     citer = FIRST_LIST(citer, next, prev)) {
			struct thd_comp *tc;

			tc = &titer->comp_info[citer->spdid];
			assert(tc && tc->c);
			
			tc->stack_misses = stkmgr_thd_blk_cnt(tid, tc->c->spdid, 0);
			tc->avg_time_blocked = stkmgr_thd_blk_time(tid, tc->c->spdid, 1);
			tc->tot_time_blocked = tc->avg_time_blocked * tc->stack_misses;
			/* } else {
			   tc->stack_misses += stkmgr_thd_blk_cnt(tid, tc->c->spdid, 0) + 1;
			   tc->avg_time_blocked += stkmgr_thd_blk_time(tid, tc->c->spdid, 1) + 1;
			   tc->tot_time_blocked += tc->avg_time_blocked * tc->stack_misses + 1;
			   tc->stack_misses >>= 1;
			   tc->avg_time_blocked >>= 1;
			   tc->tot_time_blocked >>= 1; */
			assert(tc->avg_time_blocked != (unsigned long)-1 && tc->stack_misses >= 0);
			
			if (tc->stack_misses) {
			 	printc("\tStack info for %d: time blocked %ld, misses %d\n",
			 	       tc->c->spdid, tc->avg_time_blocked, tc->stack_misses);
			}
			
		}
	}
	for (citer = FIRST_LIST(&components, next, prev) ; 
	     citer != &components ; 
	     citer = FIRST_LIST(citer, next, prev)) {
		int est;
		est = stkmgr_spd_concurrency_estimate(citer->spdid);
		assert(est != -1);
		citer->concur_est = est;
		/* if (citer->concur_est > 1) */
		/* 	printc("Spd %d concurrency estimate: %d; ", citer->spdid, citer->concur_est); */
		/*} else {
		  citer->concur_est += est + 1;
		  citer->concur_est >>= 1;
		  if (citer->concur_est > 1)
		  printc("Spd %d concurrency estimate: %d\n", citer->spdid, citer->concur_est);
		  }*/
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
collect_spare_stacks(void)
{
	struct component *c;
	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		if (c->allocated > c->concur_est) { /* take the spare stacks away */
			if (c->concur_est == 0) {
				c->concur_new = 1;
				available += c->allocated - 1;
			}
			else {
				c->concur_new = c->concur_est;
				available += c->allocated - c->concur_est;
			}
		}
	}
}

struct thd *
find_largest_tardiness(void)
{
	struct thd *p1,*p2;
	p2=FIRST_LIST(&threads, next, prev);
	for (p1=FIRST_LIST(p2,next,prev);
	     p1!=&threads;
	     p1=FIRST_LIST(p1,next,prev)){
		if (p1->tardiness > p2->tardiness)
			p2 = p1;
        }
	return p2;
}


void 
move_stack_and_update_tardiness(struct component *c_add, struct component * c_take_away)
{
	
	struct thd * iter;
	struct component * c ;
	struct thd_comp * tc;
	unsigned long atb;
	
	if (c_add) {
		c = c_add;
//		printc("Add one stack to Comp %d:(est:%d, old:%d -> new:%d)\n",c->spdid,c->concur_est,c->concur_new,c->concur_new+1);
		
		for ( iter=FIRST_LIST(&threads, next, prev);
		      iter!=&threads; /* calculate all the threads have tardiness in c_add component */
		      iter=FIRST_LIST(iter,next,prev)){
			tc = &(iter->comp_info[c->spdid]);
			atb = tc->avg_time_blocked;
			if (atb) {   
				/* printc("Thd %d tardiness: %ld   ", iter->tid, iter->tardiness); */
				/* printc("(atb : %ld ", tc->avg_time_blocked); */
				if (tc->avg_time_blocked < tc->impact)
					tc->avg_time_blocked = 0;
				else
					tc->avg_time_blocked -= tc->impact;
				/* printc("atb new: %ld) ", tc->avg_time_blocked); */
				iter->tardiness -= tc->impact;
				/* printc("predict new tardiness:%ld\n", iter->tardiness); */
			}

		}
		c->concur_new++;
		c->add_in++;
	}
	if (c_take_away) {
		c = c_take_away;
		assert(c->concur_new > 1);
//		printc("  Take away one stack from Comp %d:(est:%d, old:%d -> new:%d)\n",c->spdid,c->concur_est,c->concur_new,c->concur_new-1);
		
		for ( iter=FIRST_LIST(&threads, next, prev);
		      iter!=&threads; /* calculate all the threads have tardiness in c_take_away component */
		      iter=FIRST_LIST(iter,next,prev)){
			tc = &(iter->comp_info[c->spdid]);
			atb = tc->avg_time_blocked; 
			if (tc->impact || tc->old_impact) {
				/* printc("Thd %d tardiness: %ld   ", iter->tid, iter->tardiness); */
				/* printc("(atb : %ld ", tc->avg_time_blocked); */
				tc->avg_time_blocked += tc->impact ? tc->impact : tc->old_impact;
				/* printc("atb new: %ld) ", tc->avg_time_blocked); */
				iter->tardiness += tc->impact ? tc->impact : tc->old_impact;
				/* printc("predict new tardiness:%ld\n", iter->tardiness); */
			}
		}
		c->concur_new--;
	}
}

void 
allocate_NRT_stacks()
{
	int diff=0;
	struct component * c;
	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		diff = c->concur_est - c->concur_new;
		if (diff > 0) {
			if (diff < available) {
				c->concur_new=c->concur_est;
				available -= diff;
			} else {
				c->concur_new=c->concur_new + available;
				available=0;
				break;
			}
		} else {
			if (diff < 0 && c->concur_est){
				printc("!!!!!!!!!BUG: concur_new %d > concur_est %d!",c->concur_new,c->concur_est);
			}
		}
	}
}

void 
calc_component_tardiness(struct component * c)
{
	struct thd * titer;
	struct thd_comp * tc;
	c->add_impact = 0;
	c->remove_impact = 0;
	/* calculate the total tardiness of this component if adding one stack */ 
	for ( titer = FIRST_LIST(&threads, next, prev) ;
	      titer != &threads ; 
	      titer = FIRST_LIST(titer, next, prev)) {
		tc = &titer->comp_info[c->spdid];
		if (titer->tardiness > 0 && tc->avg_time_blocked) {
			/*tardiness = min(tardiness, blocking time)*/
			c->add_impact += (long)tc->impact < titer->tardiness ? (long)tc->impact : titer->tardiness;
		}
	}
	/* if (c->add_impact) */
	/* 	printc("          comp %d, one stack can improve total tardiness: %ld\n", c->spdid, c->add_impact); */
}

void
calc_component_max_tardiness(struct component * c)
{
	struct thd * titer;
	struct thd_comp * tc;
	long largest = 0, tmp_tardiness;
	c->add_impact = 0;
	c->remove_impact = 0;
	/* calculate the max tardiness if adding one stack to this component */ 
	for ( titer = FIRST_LIST(&threads, next, prev) ;
	      titer != &threads ; 
	      titer = FIRST_LIST(titer, next, prev)) {
		tc = &titer->comp_info[c->spdid];
		if (titer->tardiness > 0) {
			if (tc->avg_time_blocked)
				tmp_tardiness = titer->tardiness - (long)tc->impact;
			else 
				tmp_tardiness = titer->tardiness;
			if (tmp_tardiness > largest)
				largest = tmp_tardiness;
		}
	}
	c->add_impact = largest;
	/* if (c->add_impact < largest_tardiness) */
	/* 	printc("           largest tardiness: %ld if add one stack to comp %d\n",c->add_impact, c->spdid); */
}

struct component * 
find_min_tardiness_comp(struct component * c_original)
{
	struct component * c, * min_c = NULL;
	struct thd_comp * tc, * tco;
	struct thd * titer;

	if (ALGORITHM == 1) {
		long worsen, min = 0, tmp_tardiness;
		unsigned long impact_with_history;
		/* find the component that increasing the total tardiness least if take one stack to c_original */
		for( c = FIRST_LIST(&components, next, prev) ; 
		     c != &components ;
		     c = FIRST_LIST(c, next, prev)) {
			if (c->concur_new == 1 || c == c_original || c->add_in || c->spdid == 11)
				continue;
			for ( titer = FIRST_LIST(&threads, next, prev) ;
			      titer != &threads ; 
			      titer = FIRST_LIST(titer, next, prev)) {
				tc = &titer->comp_info[c->spdid];
				tco = &titer->comp_info[c_original->spdid];
				tmp_tardiness = titer->tardiness  - (long)tco->impact;
				impact_with_history = tc->impact ? tc->impact : tc->old_impact;
				if (tmp_tardiness > 0){
					c->remove_impact += impact_with_history; 
				} else {
					if (tmp_tardiness + (long)impact_with_history > 0 && c->concur_new > 1)
						c->remove_impact += tmp_tardiness + (long)impact_with_history;
				}
			}
			worsen = c->remove_impact;
			if (min == 0 || worsen < min)
			{
				min = worsen;
				min_c = c;
			}
		}
		if (min_c && min < c_original->add_impact)
			return min_c;
		else
			return NULL;
	} else {
		long largest, impact_largest, tmp_tardiness, min = 0, min_impact = 0;
		unsigned long impact_with_history;
		/* find the component that influence the max tardiness least if take one stack to c_original */
		for( c = FIRST_LIST(&components, next, prev); 
		     c != &components ;
		     c = FIRST_LIST(c, next, prev)) {
			if (c->concur_new == 1 || c == c_original || c->add_in || c->spdid == 11)
				continue;
			largest = 0;
			impact_largest = 0;
			for ( titer = FIRST_LIST(&threads, next, prev) ;
			      titer != &threads ; 
			      titer = FIRST_LIST(titer, next, prev)) {
				tc = &titer->comp_info[c->spdid];
				tco = &titer->comp_info[c_original->spdid];
				impact_with_history = tc->impact ? tc->impact : tc->old_impact;
				tmp_tardiness = titer->tardiness + (long)impact_with_history - (long)tco->impact;
				if (tmp_tardiness > largest) 
					largest = tmp_tardiness;
				if (impact_with_history && tmp_tardiness > impact_largest)
					impact_largest = tmp_tardiness;
			}
			c->remove_impact = largest;
			if (largest < min || ! min_c){
				min = largest;
				min_c = c;
				min_impact = impact_largest;
			}
			/* if multiple components impact the same to the max tardiness,
			   we choose one impact the actual tardiness least*/
			if (largest == min && impact_largest < min_impact) {
				min_c = c;
				min_impact = impact_largest;
			}
		}
		if (min < largest_tardiness && min_c)
			return min_c;
		else
			return NULL;
	}
}

struct component * 
find_tardiness_comp(void)
{
	struct component * c, * max_c = NULL;
	
	if (ALGORITHM == 1)
	{
		long max = 0;
		/* find one that improve the total tardiness most */
		for (c = FIRST_LIST(&components, next, prev) ; 
		     c != &components ;
		     c = FIRST_LIST(c, next, prev))  {			
			calc_component_tardiness(c);
			if (c->add_impact > max){
				max_c = c;
				max = c->add_impact;
			}			
		}
		if (max > 0)
			return max_c;
		else
			return NULL;
	} else {
		long min = 0;
		struct thd * t;
		while(1) {
			/* find a component that improve the max tardiness most */
			t = find_largest_tardiness();
			largest_tardiness = t->tardiness;
			if (largest_tardiness <= 0) 
				return NULL;
			for (c = FIRST_LIST(&components, next, prev) ; 
			     c != &components ;
			     c = FIRST_LIST(c, next, prev))  {			
				calc_component_max_tardiness(c);
				if (c->add_impact < min || ! max_c){
					max_c = c;
					min = c->add_impact;
				}
				if (c->add_impact == min && t->comp_info[c->spdid].impact > t->comp_info[max_c->spdid].impact) {
					max_c = c;
				}
			}
			if (t->comp_info[max_c->spdid].avg_time_blocked && t->comp_info[max_c->spdid].impact)
				break;
			else
				t->tardiness = 0; /* allocating stacks can't benefit current largest tardiness thread */
		}
		return max_c;
	}
}

static void 
calc_improvement(void)
{
	struct thd * titer;
	struct component * citer;
	struct thd_comp * tc;
	for (citer = FIRST_LIST(&components, next, prev) ; 
	     citer != &components ; 
	     citer = FIRST_LIST(citer, next, prev)) {
		citer->add_in = 0;
		for ( titer = FIRST_LIST(&threads, next, prev) ;
		      titer != &threads ; 
		      titer = FIRST_LIST(titer, next, prev)) {
			tc = &titer->comp_info[citer->spdid];
			if (tc->avg_time_blocked > 0 && tc->c->concur_est > tc->c->concur_new)
				/* improvement of one stack : time blocked / (est - allocated), +1 for round up */
				tc->impact = tc->old_impact = tc->avg_time_blocked / (tc->c->concur_est - tc->c->allocated) + 1;
			else {
				tc->impact = 0;
				tc->old_impact >>= 1;
			}
		}
	}
}

static void 
set_concur_new(void)
{
	struct component *c;

	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		assert(c->concur_new != 0);
		stkmgr_set_concurrency(c->spdid, c->concur_new, 1);
		c->allocated = c->concur_new;
	}
}

static void
policy(void)
{
	struct component * c_add, * c_get;

	//printc("Policy Start!\n");
	stkmgr_stack_report(); /* report stack usage */
	/* collect_spare_stacks(); don't collect if not necessary */
	calc_improvement();
	int count = 0, collected = 0;
	while(1){
		c_add = find_tardiness_comp();
		if (!c_add) break;
		assert(c_add->concur_est > c_add->concur_new);
		if (available > 0) { /* we have spare stacks, allocate one */
			available--;
                        move_stack_and_update_tardiness(c_add, NULL);/* add one available stack to c_add */
		}
		else { 
			/* search for spare stacks if we haven't*/
			if (!collected) {
				 collect_spare_stacks();
				 collected = 1;
				 continue;
			}
			/* no available stacks, try to take one stack from other components if necessary */
			c_get = find_min_tardiness_comp(c_add);
			if (c_get) 
				move_stack_and_update_tardiness(c_add, c_get);/* we found one */
			else 
				break;/* we shouldn't take stacks away from any where */
		}
		count++;
	}
	if (available > 0) {  /* we have spare stacks for none real-time threads */
		//allocate_NRT_stacks();
	}
	set_concur_new();
	//printc("quota left:%d, iters: %d\n", available, count);
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

		printc("STK Policy: Found thread %d.\n", i);

		c = FIRST_LIST(&components, next, prev);
		for (j = 0 ; j < ncomps ; j++) {
			assert(&components != c);
			t->comp_info[c->spdid].c = c;
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

static void
init_policy(void)
{
	struct component *c;
	int tempsetting = 20;
	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		switch (c->spdid) {
		case 11:  stkmgr_set_concurrency(c->spdid, tempsetting, 1); c->allocated = c->concur_new = tempsetting; available -= tempsetting ;break;
		default:  stkmgr_set_concurrency(c->spdid, 1, 1); c->allocated = c->concur_new = 1; available -= 1;
		}
	}
}

static void
thdpool_1_policy(void)
{
	stkmgr_stack_report();  /* report stacks usage */
	struct component *c;
	int tempsetting = 100;
	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		switch (c->spdid) {
		case 11:  stkmgr_set_concurrency(c->spdid, tempsetting, 0); break;
 		default:  stkmgr_set_concurrency(c->spdid, 1, 0);
		}
	}
}

static void
init_thdpool_max_policy(void)
{
	 /* no report when init */
	struct component *c;
	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		switch (c->spdid) {
		case 11:  stkmgr_set_concurrency(c->spdid, 100, 0); break;
		default:  stkmgr_set_concurrency(c->spdid, THD_POOL, 0);
		}
	}
}

static void
thdpool_max_policy(void)
{
	stkmgr_stack_report(); /* report stacks usage */
	struct component *c;
	for (c = FIRST_LIST(&components, next, prev) ; 
	     c != &components ;
	     c = FIRST_LIST(c, next, prev)) {
		switch (c->spdid) {
		case 11:  stkmgr_set_concurrency(c->spdid, 100, 0); break;
		default:  stkmgr_set_concurrency(c->spdid, THD_POOL, 0);
		}
	}
	
}

void 
cos_init(void *arg)
{
	stkmgr_set_concurrency(11, 100, 1);
	INIT_LIST(&threads, next, prev);
	/* Wait for all other threads to initialize */
	//timed_event_block(cos_spd_id(), 97);

	init_spds();
#ifdef THD_POOL
	if (THD_POOL != 1)
		init_thdpool_max_policy();
	else
		init_policy();
#else
	init_policy();
#endif
	periodic_wake_create(cos_spd_id(), POLICY_PERIODICITY);

	int i = 0, waiting = 100 / POLICY_PERIODICITY;
	do { 
		periodic_wake_wait(cos_spd_id());
	} while (i++ < waiting);

	init_thds();

	while (1) {
		gather_data();
		/* stkmgr_stack_report(); */
#ifdef THD_POOL
		if (THD_POOL == 1)
			thdpool_1_policy();
		else
			thdpool_max_policy();
#else
		policy();
#endif
		periodic_wake_wait(cos_spd_id());
	}
	return;
}
