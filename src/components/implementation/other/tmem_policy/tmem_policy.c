/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu. Jiguo Song,
 * songjiguo@gwu.edu. Qi Wang, interwq@gwu.edu. All rights reserved.
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

#include <cbuf_c.h>
#include <stkmgr.h>
#include <mem_pool.h>
#include <cos_debug.h>

#define DEFAULT_TMEM_AMNT 5

#include <cos_list.h>
#include <heap.h>
#include <limits.h>

//#define _DEBUG_TMEM_POLICY
#ifdef _DEBUG_TMEM_POLICY
/* #define WHERESTR "[file %s, line %d]:" */
/* #define WHEREARG __FILE__, __LINE__ */
/* #define DOUT(fmt,...) printc(WHERESTR fmt, WHEREARG, ##__VA_ARGS__) */
#define DOUT(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define DOUT(fmt,...)
#endif
 
enum{
	MAX = 0,
	AVG
};

/* ALGORITHM: 1 for minimize AVG tardiness, otherwise minimize MAX tardiness*/
#define ALGORITHM MAX
 
//#define THD_POOL MAX_NUM_MEM
//#define THD_POOL 1
#define CBUF_UNIT 5

#define POLICY_PERIODICITY 100

#define STK_MGR 0
#define CBUF_MGR 1

/* data-structures */
struct thd_sched {
	int period, priority;
	/* These change over time */
	int misses, deadlines;
	long lateness, miss_lateness;
};

struct component {
	spdid_t spdid;
	int allocated, concur_est, concur_new, concur_hist;
	int mgr; /* which tmem_mgr this component "belongs" to */
	long add_impact, remove_impact; /* the impact of adding / removing one tmem */
	int add_in; /* number of tmems we add into this component in the current period */
	int ss_counter; /* self-suspension counter*/
	struct component *next, *prev;
};

struct thd_comp {
	unsigned long avg_time_blocked, tot_time_blocked, time_per_deadline;
	/* impact and history impact for one tmem */
	unsigned long impact, old_impact;
	int tmem_misses;
	struct component *c;
};

struct thd {
	unsigned short int tid;
	struct thd_sched sched_info;
	struct thd_comp comp_info[NUM_TMEM_MGR][MAX_NUM_SPDS];
	long tardiness;
	struct thd *next, *prev;
};

struct thd threads;
struct component components[NUM_TMEM_MGR];

int ncomps = 0;
struct heap *h;

int available = MAX_NUM_MEM;

long largest_tardiness;

static inline void 
collect_cbuf_blkinfo(struct thd_comp *tc, int tid)
{
	tc->tmem_misses = cbufmgr_thd_blk_cnt(tid, tc->c->spdid, 0);
	tc->avg_time_blocked = cbufmgr_thd_blk_time(tid, tc->c->spdid, 1);
	tc->tot_time_blocked = tc->avg_time_blocked * tc->tmem_misses;
}

static inline void 
collect_stk_blkinfo(struct thd_comp *tc, int tid)
{
	tc->tmem_misses = stkmgr_thd_blk_cnt(tid, tc->c->spdid, 0);
	tc->avg_time_blocked = stkmgr_thd_blk_time(tid, tc->c->spdid, 1);
	tc->tot_time_blocked = tc->avg_time_blocked * tc->tmem_misses;
}

static inline void 
collect_stk_compinfo(struct component *citer)
{
	int est;
	est = stkmgr_spd_concurrency_estimate(citer->spdid);
	assert(est != -1);
	citer->concur_est = est;
	citer->ss_counter = stkmgr_detect_suspension(citer->spdid, 0);
	/* reset the self-suspension counter if it's greater
	 * than concurrency estimation */
	if (citer->ss_counter > est) stkmgr_detect_suspension(citer->spdid, 1);
	citer->allocated = stkmgr_get_allocated(citer->spdid);
}

static inline void 
collect_cbuf_compinfo(struct component *citer)
{
	int est;
	est = cbufmgr_spd_concurrency_estimate(citer->spdid);
	assert(est != -1);
	citer->concur_est = est;
	citer->ss_counter = cbufmgr_detect_suspension(citer->spdid, 0);
	/* reset the self-suspension counter if it's greater
	 * than concurrency estimation */
	if (citer->ss_counter > est) cbufmgr_detect_suspension(citer->spdid, 1);
	citer->allocated = cbufmgr_get_allocated(citer->spdid);
}

static void
gather_data(int counter)
{
	struct thd *titer;
	struct component *citer;
	int mgr;

	DOUT("Tmem policy: Gathering data.\n");
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
		if (counter == 0) printc("Thread %d DLM%d, %ld miss\n", tid, ts->misses, ts->miss_lateness);
		/* Component info */
		for (mgr = 0; mgr < NUM_TMEM_MGR; mgr++) {
			for (citer = FIRST_LIST(&components[mgr], next, prev) ;
			     citer != &components[mgr] ;
			     citer = FIRST_LIST(citer, next, prev)) {
				struct thd_comp *tc;

				tc = &titer->comp_info[citer->mgr][citer->spdid];
				assert(tc && tc->c);
				switch (mgr) {
				case STK_MGR:
					collect_stk_blkinfo(tc, tid);
					break;
				case CBUF_MGR:
					collect_cbuf_blkinfo(tc, tid);
					break;
				default: 
					BUG();
				}
				assert(tc->tmem_misses >= 0);
			
				if (counter == 0 && tc->tmem_misses) {
					/* printc("MGR%d Tmem info for spd %d: time blocked %ld, misses %d\n", tc->c->mgr, tc->c->spdid, tc->avg_time_blocked, tc->tmem_misses); */
				}
			}
		}
	}

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (citer = FIRST_LIST(&components[mgr], next, prev) ;
		     citer != &components[mgr] ;
		     citer = FIRST_LIST(citer, next, prev)) {
			switch (mgr) {
			case STK_MGR:
				collect_stk_compinfo(citer);
				break;
			case CBUF_MGR:
				collect_cbuf_compinfo(citer);
				break;
			default: 
				BUG();
			}
			/* printc("MGR %d, Spd %d concurrency estimate: %d;alloc %d,ss %d\n", mgr, citer->spdid, citer->concur_est, citer->allocated, citer->ss_counter); */
		}
	}
}


struct thd *
find_largest_tardiness(void)
{
	struct thd * p1,* p2;
	p2 = FIRST_LIST(&threads, next, prev);
	for (p1 = FIRST_LIST(p2,next,prev) ;
	     p1 != &threads ;
	     p1 = FIRST_LIST(p1,next,prev)) {
		if (p1->tardiness > p2->tardiness)
			p2 = p1;
        }
	return p2;
}

void
move_tmem_and_update_tardiness(struct component *c_add, struct component * c_take_away)
{
	
	struct thd * iter;
	struct component * c ;
	struct thd_comp * tc;
	unsigned long atb;
	
	if (c_add) {
		c = c_add;
		/* printc("Add one tmem to Comp %d:(est:%d, old:%d -> new:%d)\n",c->spdid,c->concur_est,c->concur_new,c->concur_new+1); */
		/* calculate all the threads have tardiness in c_add component */
		for (iter = FIRST_LIST(&threads, next, prev) ;
		     iter != &threads ;
		     iter = FIRST_LIST(iter,next,prev)) {
			tc = &(iter->comp_info[c->mgr][c->spdid]);
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
		/* printc("  Take away one tmem from Comp %d:(est:%d, old:%d -> new:%d)\n",c->spdid,c->concur_est,c->concur_new,c->concur_new-1); */
		/* calculate all the threads have tardiness in c_take_away component */
		for (iter = FIRST_LIST(&threads, next, prev) ;
		     iter != &threads ;
		     iter = FIRST_LIST(iter,next,prev)) {
			tc = &(iter->comp_info[c->mgr][c->spdid]);
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

/* void  */
/* allocate_NRT_tmems() */
/* { */
/* 	int diff=0; */
/* 	struct component * c; */
/* 	for (c = FIRST_LIST(&components, next, prev) ;  */
/* 	     c != &components ; */
/* 	     c = FIRST_LIST(c, next, prev)) { */
/* 		diff = c->concur_est - c->concur_new; */
/* 		if (diff
 > 0) { */
/* 			if (diff < available) { */
/* 				c->concur_new=c->concur_est; */
/* 				available -= diff; */
/* 			} else { */
/* 				c->concur_new=c->concur_new + available; */
/* 				available=0; */
/* 				break; */
/* 			} */
/* 		} else { */
/* 			if (diff < 0 && c->concur_est) { */
/* 				printc("BUG: concur_new %d > concur_est %d!",c->concur_new,c->concur_est); */
/* 				assert(0); */
/* 			} */
/* 		} */
/* 	} */
/* } */

struct component *
find_min_tardiness_comp(struct component * c_original)
{
	struct component * c, * min_c = NULL;
	struct thd_comp * tc, * tco;
	struct thd * titer;
	int mgr;

	if (ALGORITHM == 1) {
		long worsen, min = 0, tmp_tardiness, tot_impact, min_tot_impact = 0;
		unsigned long impact_with_history;
		/* find the component that increasing the total tardiness least if take one tmem to c_original */
		for (mgr = 0; mgr < NUM_TMEM_MGR; mgr++) {
			for (c = FIRST_LIST(&components[mgr], next, prev) ;
			     c != &components[mgr] ;
			     c = FIRST_LIST(c, next, prev)) {
				if (c->concur_new == 1 || c == c_original || c->add_in || c->ss_counter + 1 >= c->concur_new)
					continue;
				tot_impact = 0;
				for ( titer = FIRST_LIST(&threads, next, prev) ;
				      titer != &threads ;
				      titer = FIRST_LIST(titer, next, prev)) {
					tc = &titer->comp_info[c->mgr][c->spdid];
					tco = &titer->comp_info[c_original->mgr][c_original->spdid];
					tmp_tardiness = titer->tardiness  - (long)tco->impact;
					impact_with_history = tc->impact ? tc->impact : tc->old_impact;
					tot_impact += impact_with_history;
					if (tmp_tardiness > 0) {
						c->remove_impact += impact_with_history;
					} else {
						if (tmp_tardiness + (long)impact_with_history > 0 && c->concur_new > 1)
							c->remove_impact += tmp_tardiness + (long)impact_with_history;
					}
				}
				worsen = c->remove_impact;
				if (!min_c || worsen < min) {
					min = worsen;
					min_c = c;
					min_tot_impact = tot_impact;
				}
				if (worsen == min && min_tot_impact > tot_impact) {
					min_c = c;
					min_tot_impact = tot_impact;
				}
			}
		}
		if (min_c && min < c_original->add_impact)
			return min_c;
		else
			return NULL;
	} else {
		long largest, impact_largest, tmp_tardiness, min = 0, min_impact = 0;
		unsigned long impact_with_history;
		/* find the component that influence the max tardiness least if take one tmem to c_original */
		for (mgr = 0; mgr < NUM_TMEM_MGR; mgr++) {		
			for( c = FIRST_LIST(&components[mgr], next, prev);
			     c != &components[mgr] ;
			     c = FIRST_LIST(c, next, prev)) {
				if (c->concur_new == 1 || c == c_original || c->add_in || c->ss_counter + 1 >= c->concur_new)
					continue;
				largest = 0;
				impact_largest = LONG_MIN;
				for ( titer = FIRST_LIST(&threads, next, prev) ;
				      titer != &threads ;
				      titer = FIRST_LIST(titer, next, prev)) {
					tc = &titer->comp_info[c->mgr][c->spdid];
					tco = &titer->comp_info[c_original->mgr][c_original->spdid];
					impact_with_history = tc->impact ? tc->impact : tc->old_impact;
					tmp_tardiness = titer->tardiness + (long)impact_with_history - (long)tco->impact;
					if (tmp_tardiness > largest)
						largest = tmp_tardiness;
					if (impact_with_history && tmp_tardiness > impact_largest)
						impact_largest = tmp_tardiness;
				}
				c->remove_impact = largest;
				if (largest < min || ! min_c) {
					min = largest;
					min_c = c;
					min_impact = impact_largest;
				}
				/* if multiple components impact the
				 * same to the max tardiness, we
				 * choose one impact the actual
				 * tardiness least */
				if (largest == min && impact_largest < min_impact) {
					min_c = c;
					min_impact = impact_largest;
				}
			}
		}
		if (min < largest_tardiness && min_c)
			return min_c;
		else
			return NULL;
	}
}

void
calc_component_tardiness(struct component * c)
{
	struct thd * titer;
	struct thd_comp * tc;

	c->add_impact = 0;
	c->remove_impact = 0;
	/* calculate the total tardiness of this component if adding one tmem */
	for ( titer = FIRST_LIST(&threads, next, prev) ;
	      titer != &threads ;
	      titer = FIRST_LIST(titer, next, prev)) {
		tc = &titer->comp_info[c->mgr][c->spdid];
		if (titer->tardiness > 0 && tc->avg_time_blocked) {
			/*tardiness = min(tardiness, blocking time)*/
			c->add_impact += (long)tc->impact < titer->tardiness ? (long)tc->impact : titer->tardiness;
		}
	}
	/* if (c->add_impact) */
	/* 	printc("          comp %d, one tmem can improve total tardiness: %ld\n", c->spdid, c->add_impact); */
}

void
calc_component_max_tardiness(struct component * c)
{
	struct thd * titer;
	struct thd_comp * tc;
	long largest = 0, tmp_tardiness;

	c->add_impact = 0;
	c->remove_impact = 0;
	/* calculate the max tardiness if adding one tmem to this component */
	for ( titer = FIRST_LIST(&threads, next, prev) ;
	      titer != &threads ;
	      titer = FIRST_LIST(titer, next, prev)) {
		tc = &titer->comp_info[c->mgr][c->spdid];
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
	/* 	printc("           largest tardiness: %ld if add one tmem to comp %d\n",c->add_impact, c->spdid); */
}

struct component *
find_tardiness_comp(void)
{
	struct component * c, * max_c = NULL;
	int mgr;

	if (ALGORITHM == 1) {
		long max = 0;
		/* find one that improve the total tardiness most */
		for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
			for (c = FIRST_LIST(&components[mgr], next, prev) ;
			     c != &components[mgr] ;
			     c = FIRST_LIST(c, next, prev))  {
				calc_component_tardiness(c);
				if (c->add_impact > max) {
					max_c = c;
					max = c->add_impact;
				}
				if (c->add_impact == max && max > 0 && max_c != c) {
					/* if one tmem benefits multiple
					 * components the same of tardiness,
					 * we want the component with the max
					 * total block time. */
					struct thd * titer;
					unsigned long tot_impact1 = 0, tot_impact2 = 0;
					for ( titer = FIRST_LIST(&threads, next, prev) ;
					      titer != &threads ;
					      titer = FIRST_LIST(titer, next, prev)) {
						tot_impact1 += titer->comp_info[c->mgr][c->spdid].impact;
						tot_impact2 += titer->comp_info[max_c->mgr][max_c->spdid].impact;
					}
					if (tot_impact1 > tot_impact2)
						max_c = c;
				}
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
			for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
				for (c = FIRST_LIST(&components[mgr], next, prev) ;
				     c != &components[mgr] ;
				     c = FIRST_LIST(c, next, prev))  {
					calc_component_max_tardiness(c);
					if (c->add_impact < min || ! max_c) {
						max_c = c;
						min = c->add_impact;
					}
					if (c->add_impact == min && t->comp_info[c->mgr][c->spdid].impact > t->comp_info[max_c->mgr][max_c->spdid].impact) {
						max_c = c;
					}
				}
			}
			if (t->comp_info[max_c->mgr][max_c->spdid].avg_time_blocked && t->comp_info[max_c->mgr][max_c->spdid].impact)
				break;
			else
				t->tardiness = 0;
			/* allocating tmems can't benefit current largest tardiness thread */
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
	int mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) { 
		for (citer = FIRST_LIST(&components[mgr], next, prev) ;
		     citer != &components[mgr] ;
		     citer = FIRST_LIST(citer, next, prev)) {
			citer->add_in = 0;
			for ( titer = FIRST_LIST(&threads, next, prev) ;
			      titer != &threads ;
			      titer = FIRST_LIST(titer, next, prev)) {
				tc = &titer->comp_info[citer->mgr][citer->spdid];
				if (tc->avg_time_blocked > 0 && tc->c->concur_est > tc->c->allocated)
					/* improvement of one tmem : time blocked / (est - allocated), +1 for round up */
					tc->impact = tc->old_impact = tc->avg_time_blocked / (tc->c->concur_est - tc->c->allocated) + 1;
				else {
					tc->impact = 0;
					tc->old_impact >>= 1;
				}
			}
		}
	}
}

static inline void
set_concur_new(void)
{
	struct component *c;
	int mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (c = FIRST_LIST(&components[mgr], next, prev) ;
		     c != &components[mgr] ;
		     c = FIRST_LIST(c, next, prev)) {
			assert(c->concur_new != 0);
			switch (mgr) {
			case STK_MGR:
				stkmgr_set_concurrency(c->spdid, c->concur_new, 0);
				break;
			case CBUF_MGR:
				cbufmgr_set_concurrency(c->spdid, c->concur_new, 0);
				break;
			default: BUG();
			}
		}
	}
}

/* If we have spare quota, assign them according to the history record
 * we saved. */

static void
history_allocation(void)
{
	int changed;
	struct component * citer;
	int mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (citer = FIRST_LIST(&components[mgr], next, prev) ;
		     citer != &components[mgr] ;
		     citer = FIRST_LIST(citer, next, prev)) {
			if (available == 0) break;
			if (citer->concur_hist > citer->concur_new) {
				if (available > (citer->concur_hist - citer->concur_new))
					changed = citer->concur_hist - citer->concur_new;
				else
					changed = available;
				citer->concur_new += changed;
				available -= changed;
			}
		}
	}
}

/* Update the actual number of allocated tmems, save the current
 * concur_new to concur_hist for history allocation */

static void
update_allocation(void)
{
	struct component * citer;
	int mgr, diff;
	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (citer = FIRST_LIST(&components[mgr], next, prev) ;
		     citer != &components[mgr] ;
		     citer = FIRST_LIST(citer, next, prev)) {
			citer->concur_hist = citer->concur_new;
			if (citer->allocated != citer->concur_new) {
				if (citer->concur_est == 0 || citer->allocated == 0) {
					available += citer->concur_new - 1;
					citer->concur_new = 1;
				} else {
					diff = citer->allocated - citer->concur_new;

					if (diff > available) {
						citer->concur_new += available;
						available = 0;
					} else {
						available += citer->concur_new - citer->allocated;
						citer->concur_new = citer->allocated;
					}
				}
				assert(citer->concur_new);
			}
		}
	}
	assert(available >= 0);
}

/* Allocate at least ss_counter cbufs to self-suspension
 * component if not satisfied yet. */

static void
solve_suspension(void)
{

	struct thd_comp * tc;
	struct component * citer;
	struct thd * iter;
	int mgr, changed;
	unsigned long atb;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (citer = FIRST_LIST(&components[mgr], next, prev) ;
		     citer != &components[mgr] ;
		     citer = FIRST_LIST(citer, next, prev)) {
			if (!citer->ss_counter) continue;
			if (available == 0) break;
			if (citer->concur_new < citer->ss_counter) {
				if (available > (citer->ss_counter - citer->concur_new))
					changed = citer->ss_counter - citer->concur_new;
				else
					changed = available;
				citer->concur_new += changed;
				available -= changed;
				assert(citer->concur_new);
				DOUT("allocate tmems to comp %d for suspension, cnt %d. concur_new %d -> %d \n", citer->spdid, citer->ss_counter, citer->concur_new - changed, citer->concur_new);
				/* update lateness */
				for ( iter = FIRST_LIST(&threads, next, prev);
				      iter != &threads; /* calculate all the threads have tardiness in the component */
				      iter = FIRST_LIST(iter,next,prev)) {
					tc = &(iter->comp_info[citer->mgr][citer->spdid]);
					atb = tc->avg_time_blocked;
					if (atb) {
						if (tc->avg_time_blocked < tc->impact * changed) {
							iter->tardiness -= tc->avg_time_blocked;
							tc->avg_time_blocked = 0;
						} else {
							iter->tardiness -= tc->impact * changed;
							tc->avg_time_blocked -= tc->impact * changed;
						}
					}
				}
			}
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
	unsigned short int i;
	int mgr;

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

		printc("TMEM Policy: Found thread %d.\n", i);

		for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
			for (c = FIRST_LIST(&components[mgr], next, prev) ;
			     c != &components[mgr] ;
			     c = FIRST_LIST(c, next, prev)) {
				t->comp_info[c->mgr][c->spdid].c = c;
			}
		}
	}
}

static void
init_spds(void)
{
	int i, mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		INIT_LIST(&components[mgr], next, prev);
		for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
			struct component *c;
			switch (mgr) {
			case STK_MGR:
				if (-1 == stkmgr_spd_concurrency_estimate(i)) continue;
				break;
			case CBUF_MGR:
				if (-1 == cbufmgr_spd_concurrency_estimate(i)) continue;
				break;
			default: BUG();
			}

			c = malloc(sizeof(struct component));
			if (!c) BUG();
			memset(c, 0, sizeof(struct component));
			c->spdid = i;
			c->allocated = DEFAULT_TMEM_AMNT;
			c->mgr = mgr;
			INIT_LIST(c, next, prev);
			ADD_LIST(&components[mgr], c, next, prev);
			ncomps++;
		}
	}
}

static void
init_policy(void)
{
	struct component *c;
	int mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (c = FIRST_LIST(&components[mgr], next, prev) ;
		     c != &components[mgr] ;
		     c = FIRST_LIST(c, next, prev)) {
			switch (mgr) {
			case STK_MGR:
				stkmgr_set_concurrency(c->spdid, 1, 0); c->concur_new = 1; available -= 1;
				break;
			case CBUF_MGR:
				cbufmgr_set_concurrency(c->spdid, CBUF_UNIT, 0); c->concur_new = CBUF_UNIT; available -= CBUF_UNIT;
				break;
			default: BUG();
			}
		}
	}
}

#ifdef THD_POOL
static void
thdpool_1_policy(void)
{
	struct component *c;
	int mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (c = FIRST_LIST(&components[mgr], next, prev) ; 
		     c != &components[mgr] ;
		     c = FIRST_LIST(c, next, prev)) {
			switch (mgr) {
			case STK_MGR:
				if (c->ss_counter) 
					stkmgr_set_concurrency(c->spdid, INT_MAX, 0);
				else
					stkmgr_set_concurrency(c->spdid, 1, 0); /* 0 means pool 1 doesn't revoke tmems! */
				break;
			case CBUF_MGR:
				if (c->ss_counter) 
//					cbufmgr_set_concurrency(c->spdid, INT_MAX, 0);
					cbufmgr_set_concurrency(c->spdid, CBUF_UNIT, 0);
				else
					cbufmgr_set_concurrency(c->spdid, CBUF_UNIT, 0); /* 0 means pool 1 doesn't revoke tmems! */
				break;
			default: BUG();
			}
		}
	}
	//stkmgr_set_over_quota_limit(available);
}

static void
thdpool_max_policy(void)
{
	struct component *c;
	int mgr;

	for (mgr = 0 ; mgr < NUM_TMEM_MGR ; mgr++) {
		for (c = FIRST_LIST(&components[mgr], next, prev) ; 
		     c != &components[mgr] ;
		     c = FIRST_LIST(c, next, prev)) {
			switch (mgr) {
			case STK_MGR:
				if (c->ss_counter) 
					stkmgr_set_concurrency(c->spdid, INT_MAX, 1);
				else 
					stkmgr_set_concurrency(c->spdid, THD_POOL, 1);
				break;
			case CBUF_MGR:
				if (c->ss_counter) 
					cbufmgr_set_concurrency(c->spdid, INT_MAX, 1);
				else 
					cbufmgr_set_concurrency(c->spdid, THD_POOL, 1);
				break;
			default: BUG();
			}
		}
	}
	//stkmgr_set_over_quota_limit(available);
}
#endif

static void
policy(void)
{
	struct component * c_add, * c_get;
	int count = 0;

	update_allocation();

	calc_improvement();
	
	solve_suspension();

	while (1) {
		c_add = find_tardiness_comp();
		if (!c_add) break;
		assert(c_add->concur_est > c_add->concur_new);
		if (available > 0) { /* we have spare tmems, allocate one */
			available--;
                        move_tmem_and_update_tardiness(c_add, NULL);/* add one available tmem to c_add */
		} else {
			/* no available tmems, try to take one tmem from other components if necessary */
			c_get = find_min_tardiness_comp(c_add);
			if (c_get)
				move_tmem_and_update_tardiness(c_add, c_get);/* we found one */
			else
				break;/* we shouldn't take tmems away from any where */
		}
		count++;
	}
	if (available > 0) history_allocation();
	/* if (available > 0) {  /\* we have spare tmems for none real-time threads *\/ */
	/* 	allocate_NRT_tmems(); */
	/* } */
	set_concur_new();
	/* cbufmgr_set_over_quota_limit(available); */
	/* stkmgr_set_over_quota_limit(available); */
	printc("Quota left:%d, iters: %d\n", available, count);
	return;
}

void 
cos_init(void *arg)
{
	DOUT("Tmem policy running.....\n");
	INIT_LIST(&threads, next, prev);

	init_spds();

#ifdef THD_POOL
	printc("<<<Thd Pool with total %d tmems, component size %d>>>\n", MAX_NUM_MEM, THD_POOL);
	if (THD_POOL != 1)
		thdpool_max_policy();
	else
		thdpool_1_policy();
#else
	printc("<<<Now using Algorithm %d, total number of tmems:%d >>>\n", ALGORITHM, MAX_NUM_MEM);
	DOUT("Tmem policy: %d in spd %ld\n", cos_get_thd_id(), cos_spd_id());
	init_policy();
#endif

	periodic_wake_create(cos_spd_id(), POLICY_PERIODICITY);

	/* Wait for all other threads to initialize */
	int i = 0, waiting = 100 / POLICY_PERIODICITY, counter = 0, report_period = 100 / POLICY_PERIODICITY;
	do {
		periodic_wake_wait(cos_spd_id());
	} while (i++ < waiting);

	init_thds();

	//unsigned long long s,e;
	while (1) {
		if (counter++ % report_period == 0) {
			/* report tmems usage */
			cbufmgr_buf_report();
			stkmgr_stack_report();
		}
		gather_data(counter % report_period);
#ifdef THD_POOL
		if (THD_POOL == 1)
			thdpool_1_policy();
		else
			thdpool_max_policy();
#else
		//rdtscll(s);
		DOUT("POLICY starts!\n");
		policy();
		DOUT("POLICY ends!\n");
		//rdtscll(e);
		//printc("SP:%llu cycles\n",e-s);
#endif
		periodic_wake_wait(cos_spd_id());
	}
	return;
}
