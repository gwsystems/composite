/**
 * Copyright 2009, Gabriel Parmer, The George Washington University,
 * gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_sched_tk.h>
#include <cos_debug.h>

#define NUM_PRIOS 32
#define QUANTUM (1000000000UL/100UL)

static struct prio_list {
	struct sched_thd runnable;
} priorities[NUM_PRIOS];


static inline void fp_add_thd(struct sched_thd *t, unsigned short int prio)
{
	struct sched_thd *tp;

	assert(prio < NUM_PRIOS);
	assert(sched_thd_ready(t));

//	printc("add_thd: adding thread %d with priority %d to runlist. %d", 
//	      t->id, prio, 0);
	tp = &(priorities[prio].runnable);
	ADD_LIST(LAST_LIST(tp, prio_next, prio_prev), t, prio_next, prio_prev);
	sched_get_metric(t)->priority = prio;
	sched_set_thd_urgency(t, prio);
	
	return;
}

static inline void fp_new_thd(struct sched_thd *t)
{
	fp_add_thd(t, sched_get_metric(t)->priority);
}

static inline void fp_change_prio_runnable(struct sched_thd *t, unsigned short int prio)
{
	struct sched_metric *sm = sched_get_metric(t);
	struct sched_thd *head;

	assert(prio < NUM_PRIOS);

	sm->priority = prio;
	head = &priorities[prio].runnable;
	//REM_LIST(t, prio_next, prio_prev);
//	assert(EMPTY_LIST(t, prio_next, prio_prev));
	assert(!sched_thd_inactive_evt(t));
	assert(!sched_thd_blocked(t));
	if (!EMPTY_LIST(t, prio_next, prio_prev)) REM_LIST(t, prio_next, prio_prev);
	ADD_LIST(LAST_LIST(head, prio_next, prio_prev), t, prio_next, prio_prev);
	sched_set_thd_urgency(t, prio);

	return;
}

static inline void fp_move_end_runnable(struct sched_thd *t)
{
	if (sched_thd_ready(t)) {
		assert(!sched_thd_inactive_evt(t));
		assert(!sched_thd_blocked(t));
		fp_change_prio_runnable(t, sched_get_metric(t)->priority);
	}
}

/* 
 * Include an argument to tell if we should start looking after head,
 * or after the first element.
 */
static struct sched_thd *fp_find_non_suspended_list_head(struct sched_thd *head, int second)
{
	struct sched_thd *t;

	assert(!EMPTY_LIST(head, prio_next, prio_prev));
	t = FIRST_LIST(head, prio_next, prio_prev);
	if (second) t = FIRST_LIST(t, prio_next, prio_prev);
	while (t != head) {
		if (!sched_thd_suspended(t)) {
			break;
		}
		t = FIRST_LIST(t, prio_next, prio_prev);
	}
	if (t == head) {
		return NULL;
	}

	/* this assert relies on lazy evaluation: only if second == 1,
	 * do we check to make sure the returned thread is not the
	 * first one. */
	assert(t != head && (!second || t != FIRST_LIST(head, prio_next, prio_prev)));
	assert(!sched_thd_free(t));
	assert(sched_thd_ready(t));
	return t;
}

static inline struct sched_thd *fp_find_non_suspended_list(struct sched_thd *head)
{
	return fp_find_non_suspended_list_head(head, 0);
}

static struct sched_thd *fp_get_highest_prio(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *t, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		t = fp_find_non_suspended_list(head);
		if (!t) continue;
		
		assert(sched_thd_ready(t));
		assert(sched_get_metric(t));
		assert(sched_get_metric(t)->priority == i);
		assert(!sched_thd_free(t));
		assert(sched_thd_ready(t));
		return t;
	}

	return NULL;
}

static struct sched_thd *fp_get_second_highest_prio(struct sched_thd *highest)
{
	int i;
	struct sched_thd *tmp, *head;
	unsigned short int prio;

	assert(!sched_thd_free(highest));
	assert(sched_thd_ready(highest));
	assert(fp_get_highest_prio() == highest);

	/* If the next element isn't the list head, or t, return it */
	prio = sched_get_metric(highest)->priority;
	assert(prio < NUM_PRIOS);
	head = &(priorities[prio].runnable);
	assert(fp_find_non_suspended_list(head) == highest);
	/* pass in 1 to tell the function to start looking after the first item on the list (highest) */
	tmp = fp_find_non_suspended_list_head(head, 1);
	assert(tmp != highest);
	/* Another thread at same priority */
	if (tmp) {
		assert(!sched_thd_free(tmp));
		assert(sched_thd_ready(tmp));
		return tmp;
	}
	/* assumes that idle should always exist */
	assert(prio != NUM_PRIOS-1);

	for (i = prio+1 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *t, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		t = fp_find_non_suspended_list(head);
		if (!t) continue;

		assert(!sched_thd_free(t));
		assert(sched_thd_ready(t));
		assert(sched_get_metric(t)->priority == i);

		return t;
	}

	return NULL;
}

/* 
 * TODO: retrieve the threads from the graveyard when needed, and if
 * not, then make the idle thread reap these threads by killing them
 * (for which a syscall will need to be added to inv.c).
 */
static int fp_thread_remove(struct sched_thd *t)
{
	assert(t);
	REM_LIST(t, prio_next, prio_prev);
	printc("fp_kill_thd: killing %d. %d%d\n", t->id, 0,0);

	return 0;
}

static struct sched_thd *fp_schedule(struct sched_thd *c)
{
	struct sched_thd *n;

	n = fp_get_highest_prio();
	if (n && n == c) { 	/* implies c != NULL */
		n = fp_get_second_highest_prio(n);
	}
	return n;
}

static int fp_timer_tick(int ticks)
{
	return 0;
}

static int fp_time_elapsed(struct sched_thd *t, u32_t processing)
{
	struct sched_accounting *sa;

	if (NULL == t) return 0;

	sa = sched_get_accounting(t);
	if (sa->cycles >= QUANTUM) {
		sa->cycles -= QUANTUM;
		if (sched_thd_ready(t)) {
			assert(!sched_thd_inactive_evt(t));
			assert(!sched_thd_blocked(t));
			fp_move_end_runnable(t);
		}
	}
	return 0;
}

static int fp_thread_block(struct sched_thd *t)
{
	REM_LIST(t, prio_next, prio_prev);
	return 0;
}

static int fp_thread_wakeup(struct sched_thd *t)
{
	fp_move_end_runnable(t);
	return 0;
}

static int fp_thread_new(struct sched_thd *t)
{
	fp_new_thd(t);

	return 0;
}

//#include <stdlib.h>
static int fp_thread_params(struct sched_thd *t, char *p)
{
	int prio = 4;// = atoi(p);

	sched_set_thd_urgency(t, prio);
	sched_get_metric(t)->priority = prio;
	return 0;
}

extern void print_thd_invframes(struct sched_thd *t);

static void fp_runqueue_print(void)
{
	struct sched_thd *t;
	int i;

	printc("Running threads (thd, prio, cycles):\n");
	for (i = 0 ; i < NUM_PRIOS ; i++) {
		for (t = FIRST_LIST(&priorities[i].runnable, prio_next, prio_prev) ; 
		     t != &priorities[i].runnable ;
		     t = FIRST_LIST(t, prio_next, prio_prev)) {
			if (sched_get_accounting(t)->cycles) {
				printc("\t%d, %d, %lld\n", t->id, i, 
				       sched_get_accounting(t)->cycles);
				print_thd_invframes(t);
				sched_get_accounting(t)->cycles = 0;
			}
		}
	}
}

struct sched_ops fp_ops = {
	.thread_new = fp_thread_new,
	.thread_remove = fp_thread_remove,
	.runqueue_print = fp_runqueue_print,

	.schedule = fp_schedule,
	.time_elapsed = fp_time_elapsed,
	.timer_tick = fp_timer_tick,
	.thread_block = fp_thread_block,
	.thread_wakeup = fp_thread_wakeup
};

struct sched_ops *sched_initialization(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		sched_init_thd(&priorities[i].runnable, 0, THD_FREE);
	}

	return &fp_ops;
}
