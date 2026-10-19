#include <fpds.h>

#define SLM_FPRR_NPRIOS         32
#define SLM_FPRR_PRIO_HIGHEST   TCAP_PRIO_MAX
#define SLM_FPRR_PRIO_LOWEST    (SLM_FPRR_NPRIOS - 1)

#define SLM_WINDOW_HIGHEST   1000000000000 //cycles
#define SLM_WINDOW_LOWEST    1000     //cycles


struct prioqueue {
	struct ps_list_head prio[SLM_FPRR_NPRIOS];
} CACHE_ALIGNED;
struct prioqueue run_queue[NUM_CPU];

struct timer_global {
	struct heap    h; // you need to extend the heap , ring buffer in the thread
	void           *data[MAX_NUM_THREADS];
	cycles_t       current_timeout;
} CACHE_ALIGNED;
static struct timer_global __timer_globals[NUM_CPU];

static inline struct timer_global *
timer_global(void) {
	return &__timer_globals[cos_coreid()];
}

/* The timer expired */
void
slm_timer_fpds_expire(cycles_t now)
{
	struct timer_global *g = timer_global();
	g->current_timeout = now;

	/* Should we wake up the closest-timeout thread? */
	while (heap_size(&g->h) > 0) {

		struct slm_thd *tp, *th;
		struct slm_timer_thd *tt;
		struct slm_sched_thd *st;
		/* Should we wake up the closest-timeout thread? */
		tp = heap_peek(&g->h);
		assert(tp);
		tt = slm_thd_timer_policy(tp);
		st = slm_thd_sched_policy(tp);
		assert(tt && tt->timeout_idx > 0);

		/* No more threads to wake! */
		if (cycles_greater_than(tt->abs_next_processing, now)) break;
		
		/* Dequeue thread with closest wakeup */
		th = heap_highest(&g->h);
		assert(th == tp);

		tt->timeout_idx = -1;

		// Check the state
		switch(st->state) {
			case STATE_EXPENDED:
			{
				/* A thread, in its new period with no budget, wants to replenish */

				COS_TRACE("replenish(): TID: %ld Now: %llu\n", th->tid, now, 0);
				replenish(th, now);

				/* Thread can only be in expended state if it is budgeted */
				assert(tt->is_budgeted);

				break;
			}
			case STATE_READY:
			{
				/* A thread, waiting for the execution in ready state */

				// There should not be any timer for the threads in ready state
				assert(0);

				break;
			}
			case STATE_BLOCKED:
			{
				/* A thread, blocked by the user before, wants to wake up */
				slm_thd_wakeup(th, 1);
				break;
			}
			case STATE_BLOCKED_PERIODIC:
			{
				/* A thread, blocked in the previous period wants to wake up in its next period */
				/* TODO: Not throughly tested or used, remove this comment after verification */
				slm_sched_fpds_wakeup_periodic(th, now);
				break;
			}
			case STATE_RUNNING:
			{
				/* A thread in the runqueue, executed in its current period wants to replenish */
								
				// Optimization: For the budgeted threads that still have budget(in the runqueue), 
				// We can replenish just before it is scheduled

				// replenish(th, now);

				break;
			}
			default:
				break;
		}
	}

}

/*
 * Timeout and wakeup functionality
 *
 * TODO: Replace the in-place heap with a rb-tree to avoid external, static allocation.
 */

int
slm_timer_fpds_add(struct slm_thd *t, cycles_t absolute_timeout) 
{
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);
	struct timer_global *g = timer_global();

	assert(tt && tt->timeout_idx == -1);
	assert(heap_size(&g->h) < MAX_NUM_THREADS);

	tt->abs_next_processing = absolute_timeout;
	heap_add(&g->h, t);
 
	return 0;
}

int
slm_timer_fpds_cancel(struct slm_thd *t)
{
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);
	struct timer_global *g   = timer_global();

	if (tt->timeout_idx == -1) return 0;

	assert(heap_size(&g->h));
	assert(tt->timeout_idx > 0);

	heap_remove(&g->h, tt->timeout_idx);
	tt->timeout_idx = -1;

	return 0;
}

int
slm_timer_fpds_thd_init(struct slm_thd *t)
{
	struct timer_global *g = timer_global();
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);

	*tt = (struct slm_timer_thd){
		.timeout_idx = -1,
		.abs_next_processing  = 0,
		.abs_period_start = slm_now(),
		.abs_period_end = tt->abs_period_start + SLM_WINDOW_HIGHEST,
		.budget      = 0,
		.initial_budget = 0,
		.is_budgeted = 0,
		.period      = SLM_WINDOW_HIGHEST,
	};

	COS_TRACE("slm_timer_fpds_thd_init(): TID: %ld Period Start: %llu\n", t->tid, tt->abs_period_start, 0);

	// TODO: Check if the thread has higher priority than the current thread?
	// Add timer interrupt if necessary?

	return 0;
}

void
slm_timer_fpds_thd_deinit(struct slm_thd *t)
{
	// Cancel the timers
	slm_timer_fpds_cancel(t);
	return;
}

static int
__slm_timeout_compare_min(void *a, void *b)
{
	/* FIXME: logic for wraparound in either timeout_cycs */
	return slm_thd_timer_policy((struct slm_thd *)a)->abs_next_processing <= slm_thd_timer_policy((struct slm_thd *)b)->abs_next_processing;
}

static void
__slm_timeout_update_idx(void *e, int pos)
{ slm_thd_timer_policy((struct slm_thd *)e)->timeout_idx = pos; }

static void
slm_policy_timer_init(microsec_t period)
{
	struct timer_global *g = timer_global();
	cycles_t next_timeout;

	memset(g, 0, sizeof(struct timer_global));
	heap_init(&g->h, MAX_NUM_THREADS, __slm_timeout_compare_min, __slm_timeout_update_idx);

	next_timeout = slm_now();
	g->current_timeout = next_timeout;
	slm_timeout_set(next_timeout);
}

int
slm_timer_fpds_init(void)
{
	/* 10ms */
	slm_policy_timer_init(10000);

	return 0;
}

void
slm_sched_fpds_execution(struct slm_thd *t, cycles_t cycles, cycles_t now)
{
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);
	struct slm_sched_thd *st = slm_thd_sched_policy(t);

	if(tt->is_budgeted == 0) {
		return;
	}

	tt->budget -= cycles;

	// Are period_start and period_end correct?
	assert(tt->abs_period_start <= now);
	// Did we miss the deadline? 
	// assert(tt->abs_period_end >= now + remaining WCET);
	
	// Plan the next replenishment
	// TODO: Temporary for deferrable server

	// if budget is 0, add timer and block	
	if (tt->budget <= 0) {
		//COS_TRACE("expended(): TID: %ld Now: %llu\n", t->tid, now, 0);
		expended(t);
	}

	return; 
}

void
set_next_timer_interrupt(struct slm_thd *t, cycles_t now)
{
	struct timer_global *g = timer_global();
	cycles_t next_timeout = 0; 

	/* Are there any thread in timer queue? */
	/* TODO: We dont pay attention to the priority now */
	if (heap_size(&g->h) > 0) {

		struct slm_thd *tp;
		struct slm_timer_thd *tt;
		/* What is the closest-timeout? */
		tp = heap_peek(&g->h);
		assert(tp);
		tt = slm_thd_timer_policy(tp);
		assert(tt && tt->timeout_idx > 0);

		next_timeout = tt->abs_next_processing;
		
	}
	
	/* Check if the next timeout is further than the budget of the current thread */
	if(t != NULL) { 
		struct slm_timer_thd *curr = slm_thd_timer_policy(t);
		if (curr->is_budgeted) {
			assert(curr->budget >= 0);
			//Check if the budget exceeds the abs_period_end
			//If it does, set the curr_deadline to abs_period_end
			cycles_t curr_deadline = (cycles_t)curr->budget > curr->abs_period_end ? curr->abs_period_end : (cycles_t)curr->budget;
			//Take the minimum of the next_timeout and curr_deadline
			next_timeout = (next_timeout > (curr_deadline + now)) ? (curr_deadline + now) : next_timeout;
		}
	}
	
	// TODO: Hacked because even clearing timeout, it continues to interrupt
	// slm_timeout_clear();
	slm_timeout_set(9999999999999999);

	/* Set the next timeout */
	if (next_timeout != 0) {
		g->current_timeout = next_timeout;
		slm_timeout_set(next_timeout);
	}
}

struct slm_thd *
slm_sched_fpds_schedule(cycles_t now)
{
	int i;
	struct slm_sched_thd *st;
	struct slm_timer_thd *tt;
	struct ps_list_head *prios = run_queue[cos_cpuid()].prio;
	struct timer_global *g = timer_global();

	for (i = 0 ; i < SLM_FPRR_NPRIOS ; i++) {
		if (ps_list_head_empty(&prios[i])) continue;
		st = ps_list_head_first_d(&prios[i], struct slm_sched_thd);
		tt = slm_thd_timer_policy(slm_thd_from_sched(st));

		/*
		 * We want to move the selected thread to the back of the list.
		 * Otherwise it won't be truly round robin 
		 */

		/* Threads with no budget should not be in the runqueue */
		assert(st->state != STATE_EXPENDED);
		assert(!tt->is_budgeted || tt->budget > 0);

		ps_list_rem_d(st);
		ps_list_head_append_d(&prios[i], st);

		/* Set the timer */
		set_next_timer_interrupt(slm_thd_from_sched(st), now);

		st->state = STATE_RUNNING;	
		//COS_TRACE("slm_sched_fpds_schedule(): TID: %ld\n", slm_thd_from_sched(st)->tid, 0, 0);

		return slm_thd_from_sched(st);
	}

	set_next_timer_interrupt(NULL, now);
	//COS_TRACE("slm_sched_fpds_schedule(): TID: %ld Next Timeout: %llu\n", 0, now + g->period, 0);

	return NULL;
}

int
slm_sched_fpds_block(struct slm_thd *t)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);

	assert(st->state != STATE_BLOCKED);
	assert(st->state != STATE_BLOCKED_PERIODIC);

	//COS_TRACE("slm_sched_fpds_block(): TID: %ld State: %d\n", t->tid, st->state, 0);

	/* Remove from runqueue */
	ps_list_rem_d(st);
	st->state = STATE_BLOCKED;

	// TODO: Now cancelling the timer is in sched/main.c should we move it here?

	return 0;
}

/* TODO: Not throughly tested or used, remove this comment after verification */
int
slm_sched_fpds_block_periodic(struct slm_thd *t)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);


	assert(tt->is_budgeted);
	assert(tt->abs_next_processing >= tt->abs_period_end);

	assert(st->state != STATE_BLOCKED);
	assert(st->state != STATE_BLOCKED_PERIODIC);

	/* Remove from runqueue */
	st->state = STATE_BLOCKED_PERIODIC;
	ps_list_rem_d(st);

	/* Update abs_period_start, abs_period_end and abs_next_processing */
	tt->abs_period_start = tt->abs_period_end;
	tt->abs_period_end = tt->abs_period_start + tt->period;

	// Optimization: Set the next processing time to the first replenishment time
	// TODO: Temporary for deferrable server 
	assert(tt->abs_next_processing == tt->abs_period_start);

	return 0;
}

void
expended(struct slm_thd *t)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);
	int ret = -1;

	// Remove from runqueue, note that slm_state is still RUNNING
	ps_list_rem_d(st);
	st->state = STATE_EXPENDED;
		
	// Update abs_period_start, abs_period_end
	tt->abs_period_start = tt->abs_period_start + tt->period;
	tt->abs_period_end = tt->abs_period_start + tt->period;

	// Add replenishment timer
	ret = slm_timer_fpds_add(t, tt->abs_period_start);

	assert(ret == 0);
}

void
replenish(struct slm_thd *t, cycles_t now)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);
	int ret = -1;

	tt->budget = tt->initial_budget;

	assert(st->state == STATE_EXPENDED);
	st->state = STATE_READY;

	/* Add to the runqueue */
	ps_list_head_append_d(&run_queue[cos_cpuid()].prio[t->priority], st);
}

int
slm_sched_fpds_wakeup(struct slm_thd *t)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);
	struct timer_global *g = timer_global();

	assert(ps_list_singleton_d(st));
	assert(st->state == STATE_BLOCKED);
	
	if (tt->is_budgeted) {
		/* Shift abs_period_start, abs_period_end and abs_next_processing */
		/* This prevents a thread from gaining advantage over other same priority */
		cycles_t offset_abs_next_processing = tt->abs_next_processing - tt->abs_period_start;
		 
		int periods_passed = (g->current_timeout - tt->abs_period_start) / tt->period;
		COS_TRACE("wakeup(): TID: %ld Periods Passed: %d\n", t->tid, periods_passed, 0);
		tt->abs_period_start += (periods_passed * tt->period);
		tt->abs_period_end = tt->abs_period_start + tt->period;

		/* TODO: Update replenisment window abs values */
		/* Add the cancelled timer in slm_sched_fpds_block() */
		// Recover last state
		// tt->abs_next_processing = tt->abs_period_start + offset_abs_next_processing;

		// TODO: For only deferable server 
		// If there is no budget change state and add timer
		if (tt->budget <= 0) {
			slm_timer_fpds_add(t, tt->abs_period_end);
			st->state = STATE_EXPENDED;
			return 0;
		}
	}
	/* Add to the runqueue */
	st->state = STATE_READY;
	ps_list_head_append_d(&run_queue[cos_cpuid()].prio[t->priority - 1], st);

	return 0;
}


/* TODO: Not throughly tested or used, remove this comment after verification */
int
slm_sched_fpds_wakeup_periodic(struct slm_thd *t, cycles_t now)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);

	assert(ps_list_singleton_d(st));
	assert(st->state == STATE_BLOCKED_PERIODIC);

	assert(now < tt->abs_period_end);
	replenish(t, now);

	/* Add to the runqueue */
	st->state = STATE_READY;
	ps_list_head_append_d(&run_queue[cos_cpuid()].prio[t->priority - 1], st);

	return 0;
}

void
slm_sched_fpds_yield(struct slm_thd *t, struct slm_thd *yield_to)
{

	// TODO: Not implemented yet
	assert(0);

	struct slm_sched_thd *st = slm_thd_sched_policy(t);

	ps_list_rem_d(st);
	ps_list_head_append_d(&run_queue[cos_cpuid()].prio[t->priority], slm_thd_sched_policy(yield_to));
}

int
slm_sched_fpds_thd_init(struct slm_thd *t)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);

	t->priority = SLM_FPRR_PRIO_LOWEST;
	st->state = STATE_READY;

	ps_list_init_d(st);

	return 0;
}

void
slm_sched_fpds_thd_deinit(struct slm_thd *t)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);

	// Remove from runqueue
	st->state = STATE_DEINIT;
	ps_list_rem_d(slm_thd_sched_policy(t));
}

static void
update_queue(struct slm_thd *t, tcap_prio_t prio)
{
	struct slm_sched_thd *st = slm_thd_sched_policy(t);

	t->priority = prio;
	ps_list_rem_d(st); /* if we're already on a list, and we're updating priority */
	ps_list_head_append_d(&run_queue[cos_cpuid()].prio[prio], st);

	return;
}

static void
update_period(struct slm_thd *t, cycles_t period)
{
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);

	tt->period = period;
	tt->abs_period_end = tt->abs_period_start + period;

	COS_TRACE("update_period(): TID: %lu Period: %llu\n", t->tid, tt->period, 0);

	return;
}

static void
update_budget(struct slm_thd *t, cycles_t budget)
{
	struct slm_timer_thd *tt = slm_thd_timer_policy(t);

	tt->budget = budget;
	tt->initial_budget = budget;
	tt->is_budgeted = 1;

	COS_TRACE("update_budget(): TID: %lu Budget: %llu\n", t->tid, tt->budget, 0);

	return;
}

int
slm_sched_fpds_thd_update(struct slm_thd *t, sched_param_type_t type, unsigned int v)
{

	switch (type) {
	case SCHEDP_INIT_PROTO:
	{
		update_queue(t, 0);

		return 0;
	}
	case SCHEDP_INIT:
	{
		update_queue(t, SLM_FPRR_PRIO_LOWEST);

		return 0;
	}
	case SCHEDP_PRIO:
	{
		assert(v >= SLM_FPRR_PRIO_HIGHEST && v <= SLM_FPRR_PRIO_LOWEST);
		update_queue(t, v);

		return 0;
	}
	case SCHEDP_BUDGET:
	{
		update_budget(t, v);

		return 0;
	}
	case SCHEDP_WINDOW:
	{
		assert(v <= SLM_WINDOW_HIGHEST && v >= SLM_WINDOW_LOWEST);
		update_period(t, v);

		return 0;
	}
	default:
		return -1;
	}
}

void
slm_sched_fpds_init(void)
{
	int i;

	for (i = 0 ; i < SLM_FPRR_NPRIOS ; i++) {
		ps_list_head_init(&run_queue[cos_cpuid()].prio[i]);
	}
}
