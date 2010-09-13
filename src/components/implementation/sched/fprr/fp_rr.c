/**
 * Copyright 2009, Gabriel Parmer, The George Washington University,
 * gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_sched_tk.h>
#include <cos_debug.h>

#define NUM_PRIOS    32
#define PRIO_LOW     (NUM_PRIOS-2)
#define PRIO_LOWEST  (NUM_PRIOS-1)
#define PRIO_HIGHEST 0
#define QUANTUM CYC_PER_TICK

/* runqueue */
static struct prio_list {
	struct sched_thd runnable;
} priorities[NUM_PRIOS];

/* bit-mask describing which priorities are active */
u32_t active = 0;
static inline void mask_set(unsigned short int p) { active |= 1 << p; }
static inline void mask_unset(unsigned short int p) { active &= ~(1 << p); }
static inline unsigned short int mask_high(void) 
{ 
	u32_t v = active;
	unsigned short int r = 0; 
	/* Assume 2s compliment here.  Could instead do a check for
	 * while (v & 1)..., but that's another op in the main loop */
	v = v & -v; 		/* only set least signif bit */
	while (v != 1) {
		v >>= 1;
		r++;
	}
	return r;
}


static inline void fp_move_end_runnable(struct sched_thd *t)
{
	struct sched_thd *head;
	unsigned short int p = sched_get_metric(t)->priority;

	assert(sched_thd_ready(t));
	head = &priorities[p].runnable;
	REM_LIST(t, prio_next, prio_prev);
	ADD_LIST(LAST_LIST(head, prio_next, prio_prev), t, prio_next, prio_prev);
	mask_set(p);
}

static inline void fp_add_start_runnable(struct sched_thd *t)
{
	struct sched_thd *head;
	u16_t p = sched_get_metric(t)->priority;

	assert(sched_thd_ready(t));
	head = &priorities[p].runnable;
	ADD_LIST(head, t, prio_next, prio_prev);
	mask_set(p);
}

static inline void fp_add_thd(struct sched_thd *t, unsigned short int prio)
{
	assert(prio < NUM_PRIOS);
	assert(sched_thd_ready(t));

	sched_get_metric(t)->priority = prio;
	sched_set_thd_urgency(t, prio);
	fp_move_end_runnable(t);
	mask_set(prio);

	return;
}

static inline void fp_rem_thd(struct sched_thd *t)
{
	u16_t p = sched_get_metric(t)->priority;

	/* no other thread at this priority? */
	if (t->prio_next == t->prio_prev) mask_unset(p);
	REM_LIST(t, prio_next, prio_prev);
}

static struct sched_thd *fp_get_highest_prio(void)
{
	struct sched_thd *t, *head;
	u16_t p = mask_high();

	head = &(priorities[p].runnable);
	t = FIRST_LIST(head, prio_next, prio_prev);
	assert(t != head);
	
	assert(sched_thd_ready(t));
	assert(sched_get_metric(t));
	assert(sched_get_metric(t)->priority == p);
	assert(!sched_thd_free(t));
	assert(sched_thd_ready(t));
	
	return t;
}

static struct sched_thd *fp_get_second_highest_prio(struct sched_thd *highest)
{
	struct sched_thd *t;

	fp_rem_thd(highest);
	t = fp_get_highest_prio();
	fp_add_start_runnable(highest);

	return t;
}

struct sched_thd *schedule(struct sched_thd *t)
{
	struct sched_thd *n;

	assert(!t || !sched_thd_member(t));
	n = fp_get_highest_prio();
	assert(n);
	if (n != t) return n;
	
	assert(t);
	return fp_get_second_highest_prio(n);
}

void thread_new(struct sched_thd *t)
{
	assert(t);
	fp_add_thd(t, sched_get_metric(t)->priority);
}

void thread_remove(struct sched_thd *t)
{
	assert(t);
	fp_rem_thd(t);
}

void time_elapsed(struct sched_thd *t, u32_t processing_time)
{
	struct sched_accounting *sa;

	assert(t);
	sa = sched_get_accounting(t);
	sa->cycles += processing_time;
	if (sa->cycles >= QUANTUM) {
		sa->cycles -= QUANTUM;
		/* round robin */
		if (sched_thd_ready(t)) {
			assert(!sched_thd_inactive_evt(t));
			assert(!sched_thd_blocked(t));
			fp_move_end_runnable(t);
		}
		sa->ticks++;
	}
}

void timer_tick(int num_ticks)
{
	/* see time_elapsed for time mgmt */
}

void thread_block(struct sched_thd *t)
{
	assert(t);
	assert(!sched_thd_member(t));
	fp_rem_thd(t);
}

void thread_wakeup(struct sched_thd *t)
{
	assert(t);
	assert(!sched_thd_member(t));
	fp_move_end_runnable(t);
}

#include <stdlib.h> 		/* atoi */
static int fp_thread_params(struct sched_thd *t, char *p)
{
	int prio, tmp;
	char curr = p[0];
	struct sched_thd *c;
	
	assert(t);
	switch (curr) {
	case 'r':
		/* priority relative to current thread */
		c = sched_get_current();
		assert(c);
		tmp = atoi(&p[1]);
		prio = sched_get_metric(c)->priority + tmp;
		if (prio > PRIO_LOWEST) prio = PRIO_LOWEST;
		break;
	case 'a':
		/* absolute priority */
		prio = atoi(&p[1]);
		break;
	case 'i':
		/* idle thread */
		prio = PRIO_LOWEST;
		break;
	case 't':
		/* timer thread */
		prio = PRIO_HIGHEST;
		break;
	default:
		printc("unknown priority option @ %s, setting to low\n", p);
		prio = PRIO_LOW;
	}

	sched_set_thd_urgency(t, prio);
	sched_get_metric(t)->priority = prio;

	return 0;
}

int thread_params_set(struct sched_thd *t, char *params)
{
	assert(t && params);
	return fp_thread_params(t, params);
}

extern void print_thd_invframes(struct sched_thd *t);

void runqueue_print(void)
{
	struct sched_thd *t;
	int i;

	printc("Running threads (thd, prio, ticks):\n");
	for (i = 0 ; i < NUM_PRIOS ; i++) {
		for (t = FIRST_LIST(&priorities[i].runnable, prio_next, prio_prev) ; 
		     t != &priorities[i].runnable ;
		     t = FIRST_LIST(t, prio_next, prio_prev)) {
			struct sched_accounting *sa = sched_get_accounting(t);
			unsigned long diff = sa->ticks - sa->prev_ticks;

			if (diff) {
				printc("\t%d, %d, %ld\n", t->id, i, diff);
				print_thd_invframes(t);
				sa->prev_ticks = sa->ticks;
			}
		}
	}
}

void sched_initialization(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		sched_init_thd(&priorities[i].runnable, 0, THD_FREE);
	}
}
