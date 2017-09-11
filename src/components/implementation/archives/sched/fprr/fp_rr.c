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
#define PRIO_SECOND_HIGHEST (PRIO_HIGHEST + 1)
#define IPI_HANDLER_PRIO PRIO_SECOND_HIGHEST
#define QUANTUM ((unsigned int)CYC_PER_TICK)

/* runqueue */
struct prio_list {
	struct sched_thd runnable;
};

struct fprr_per_core {
	struct prio_list priorities[NUM_PRIOS];
	/* bit-mask describing which priorities are active */
	u32_t active;

#ifdef DEFERRABLE
	unsigned long ticks;
	struct sched_thd servers;
#endif
} CACHE_ALIGNED;

PERCPU_ATTR(static, struct fprr_per_core, fprr_state);

static inline void mask_set(unsigned short int p) { PERCPU_GET(fprr_state)->active |= 1 << p; }
static inline void mask_unset(unsigned short int p) { PERCPU_GET(fprr_state)->active &= ~(1 << p); }
static inline unsigned short int mask_high(void) 
{ 
	u32_t v = PERCPU_GET(fprr_state)->active;
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
	assert(!sched_thd_suspended(t));
	head = &PERCPU_GET(fprr_state)->priorities[p].runnable;
	REM_LIST(t, prio_next, prio_prev);
	ADD_LIST(LAST_LIST(head, prio_next, prio_prev), t, prio_next, prio_prev);
	mask_set(p);
}

static inline void fp_add_start_runnable(struct sched_thd *t)
{
	struct sched_thd *head;
	u16_t p = sched_get_metric(t)->priority;

	assert(sched_thd_ready(t));
	head = &PERCPU_GET(fprr_state)->priorities[p].runnable;
	ADD_LIST(head, t, prio_next, prio_prev);
	mask_set(p);
}

static inline void fp_add_thd(struct sched_thd *t, unsigned short int prio)
{
	assert(prio < NUM_PRIOS);
	assert(sched_thd_ready(t));
	assert(!sched_thd_suspended(t));

	sched_get_metric(t)->priority = prio;
	sched_set_thd_urgency(t, prio);
	fp_move_end_runnable(t);

	return;
}

static inline void fp_rem_thd(struct sched_thd *t)
{
	u16_t p = sched_get_metric(t)->priority;

	/* if on a list _and_ no other thread at this priority? */
	if (!EMPTY_LIST(t, prio_next, prio_prev) && 
	    t->prio_next == t->prio_prev) {
		mask_unset(p);
	}
	REM_LIST(t, prio_next, prio_prev);
}

static struct sched_thd *fp_get_highest_prio(void)
{
	struct sched_thd *t, *head;
	u16_t p = mask_high();

	head = &(PERCPU_GET(fprr_state)->priorities[p].runnable);
	t = FIRST_LIST(head, prio_next, prio_prev);
	assert(t != head);
	assert(sched_thd_ready(t));
	assert(sched_get_metric(t));
	assert(sched_get_metric(t)->priority == p);
	assert(!sched_thd_free(t));
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
//	fp_add_thd(t, sched_get_metric(t)->priority);
}

void thread_remove(struct sched_thd *t)
{
	assert(t);
	fp_rem_thd(t);
	REM_LIST(t, sched_next, sched_prev);
}

void time_elapsed(struct sched_thd *t, u32_t processing_time)
{
	struct sched_accounting *sa;

	assert(t);
	sa = sched_get_accounting(t);
	sa->pol_cycles += processing_time;
	sa->cycles     += processing_time;
	if (sa->cycles >= QUANTUM) {
		while (sa->cycles > QUANTUM) {
			sa->cycles -= QUANTUM;
			sa->ticks++;
		}
		/* round robin */
		if (sched_thd_ready(t) && !sched_thd_suspended(t)) {
			assert(!sched_thd_inactive_evt(t));
			assert(!sched_thd_blocked(t));
			fp_move_end_runnable(t);
		}
#ifdef DEFERRABLE
#endif
	}
	if (sa->pol_cycles > QUANTUM) {
		sa->pol_cycles -= QUANTUM;
		if (sa->T) {
			sa->C_used++;
			if (sa->C_used >= sa->C) {
				sched_set_thd_urgency(t, NUM_PRIOS);
				if (sched_thd_ready(t)) fp_rem_thd(t);
				t->flags |= THD_SUSPENDED;
			}
		}
	}
}

void timer_tick(int num_ticks)
{
/* see time_elapsed for time mgmt */
#ifdef DEFERRABLE
	{
		struct sched_thd *t;

		assert(num_ticks > 0);
		PERCPU_GET(fprr_state)->ticks += num_ticks;
		for (t = FIRST_LIST(&PERCPU_GET(fprr_state)->servers, sched_next, sched_prev) ;
		     t != &PERCPU_GET(fprr_state)->servers                                    ;
		     t = FIRST_LIST(t, sched_next, sched_prev))
		{
			struct sched_accounting *sa = sched_get_accounting(t);
			unsigned long T_exp = sa->T_exp, T = sa->T;
			assert(T);

			if (T_exp <= PERCPU_GET(fprr_state)->ticks) {
				unsigned long off = T - (PERCPU_GET(fprr_state)->ticks % T);

				//printc("(%ld+%ld/%ld @ %ld)\n", sa->C_used, (unsigned long)sa->pol_cycles, T, T_exp);
				sa->T_exp  = PERCPU_GET(fprr_state)->ticks + off;
				sa->C_used = 0;
//				sa->pol_cycles = 0;
				if (sched_thd_suspended(t)) {
					t->flags &= ~THD_SUSPENDED;
					if (sched_thd_ready(t)) {
						fp_add_thd(t, sched_get_metric(t)->priority);
					}
				}
			}
		}
	}
#endif
}

void thread_block(struct sched_thd *t)
{
	assert(t);
	assert(!sched_thd_member(t));
	fp_rem_thd(t);
	//if (!sched_thd_suspended(t)) fp_rem_thd(t);
}

void thread_wakeup(struct sched_thd *t)
{
	assert(t);
	assert(!sched_thd_member(t));
	if (!sched_thd_suspended(t)) fp_add_thd(t, sched_get_metric(t)->priority);
}

#include <stdlib.h> 		/* atoi */

#ifdef DEFERRABLE
static int
ds_extract_nums(char *s, int *amnt)
{
	char tmp[11];
	int i;

	tmp[10] = '\0';
	for (i = 0 ; s[i] >= '0' && s[i] <= '9' && i < 10 ; i++) {
		tmp[i] = s[i];
	}
	tmp[i] = '\0';
	*amnt = i;

	return atoi(tmp);
}

static int
ds_parse_params(struct sched_thd *t, char *s)
{
	int n, prio;

	assert(s[0] == 'd');
	s++;
	prio = ds_extract_nums(s, &n);
	s += n;

	assert(s[0] == 'c');
	s++;
	sched_get_accounting(t)->C = ds_extract_nums(s, &n);
	s += n;
	sched_get_accounting(t)->C_used = 0;

	assert(s[0] == 't');
	s++;
	sched_get_accounting(t)->T = ds_extract_nums(s, &n);
	sched_get_accounting(t)->T_exp = 0;

	return prio;
}
#endif

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
		memcpy(sched_get_accounting(t), sched_get_accounting(c), sizeof(struct sched_accounting));
#ifdef DEFERRABLE
		if (sched_get_accounting(t)->T) ADD_LIST(&PERCPU_GET(fprr_state)->servers, t, sched_next, sched_prev);
#endif

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
#ifdef DEFERRABLE
	case 'd':
	{
		prio = ds_parse_params(t, p);
		if (EMPTY_LIST(t, sched_next, sched_prev) && 
		    sched_get_accounting(t)->T) {
			ADD_LIST(&PERCPU_GET(fprr_state)->servers, t, sched_next, sched_prev);
		}
		fp_move_end_runnable(t);
		break;
	}
#endif
	default:
		printc("unknown priority option @ %s, setting to low\n", p);
		prio = PRIO_LOW;
	}
	if (sched_thd_ready(t)) fp_rem_thd(t);
	fp_add_thd(t, prio);

	return 0;
}

int thread_params_set(struct sched_thd *t, char *params)
{
	assert(t && params);
	return fp_thread_params(t, params);
}


int
thread_param_set(struct sched_thd *t, struct sched_param_s *ps)
{
	unsigned int prio = PRIO_LOWEST;
	struct sched_thd *c = sched_get_current();
	
	assert(t);
	while (ps->type != SCHEDP_NOOP) {
		switch (ps->type) {
		case SCHEDP_RPRIO:
		case SCHEDP_RLPRIO:
			/* The relative priority has been converted to absolute priority in relative_prio_convert(). */
			prio = ps->value;
			/* FIXME: When the IPI handling thread is
			 * creating a thread (requested by a remote
			 * core) , since we can't copy accounting info
			 * from the actual parent (which is on a
			 * different core), we zero the accounting
			 * info instead of touching remote
			 * data-structures. */
			if (sched_curr_is_IPI_handler())
				sched_clear_accounting(t);
			else
				memcpy(sched_get_accounting(t), sched_get_accounting(c), sizeof(struct sched_accounting));
#ifdef DEFERRABLE
			if (sched_get_accounting(t)->T) ADD_LIST(&PERCPU_GET(fprr_state)->servers, t, sched_next, sched_prev);
#endif
			if (prio > PRIO_LOWEST) prio = PRIO_LOWEST;
			break;
		case SCHEDP_PRIO:
			/* absolute priority */
			prio = ps->value;
			break;
		case SCHEDP_IDLE:
			/* idle thread */
			prio = PRIO_LOWEST;
			break;
		case SCHEDP_INIT:
			/* idle thread */
			prio = PRIO_LOW;
			break;
		case SCHEDP_TIMER:
			/* timer thread */
			prio = PRIO_HIGHEST;
			break;
		case SCHEDP_IPI_HANDLER:
			prio = IPI_HANDLER_PRIO;
			break;
		case SCHEDP_CORE_ID:
			assert(ps->value == cos_cpuid());
			break;
#ifdef DEFERRABLE
		case SCHEDP_BUDGET:
			prio = sched_get_metric(t)->priority;
			sched_get_accounting(t)->C = ps->value;
			sched_get_accounting(t)->C_used = 0;
			fp_move_end_runnable(t);
			break;
		case SCHEDP_WINDOW:
			prio = sched_get_metric(t)->priority;
			sched_get_accounting(t)->T = ps->value;
			sched_get_accounting(t)->T_exp = 0;
			if (EMPTY_LIST(t, sched_next, sched_prev) && 
			    sched_get_accounting(t)->T) {
				ADD_LIST(&PERCPU_GET(fprr_state)->servers, t, sched_next, sched_prev);
			}
			fp_move_end_runnable(t);
			break;
#endif
		default:
			printc("fprr: core %ld received unknown priority option\n", cos_cpuid());
			prio = PRIO_LOW;
		}
		ps++;
	}
	/* printc("fprr: cpu %ld has new thd %d @ prio %d\n", cos_cpuid(), t->id, prio); */
	if (sched_thd_ready(t)) fp_rem_thd(t);

	fp_add_thd(t, prio);
	
	return 0;
}

int 
thread_resparams_set(struct sched_thd *t, res_spec_t rs)
{
#ifdef DEFERRABLE
	if (rs.a < 0 || rs.w < 0 || rs.a > rs.w) return -1;
	sched_get_accounting(t)->C = rs.a;
	sched_get_accounting(t)->C_used = 0;
	sched_get_accounting(t)->T = rs.w;
	sched_get_accounting(t)->T_exp = 0;
#endif
	return 0;
}

struct sched_thd *
sched_get_thread_in_spd_from_runqueue(spdid_t spdid, spdid_t target, int index)
{
	struct sched_thd *t;
	int i, cnt = 0;
	/* copied from runqueue_print, a better way would use a visitor */

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		for (t = FIRST_LIST(&PERCPU_GET(fprr_state)->priorities[i].runnable, prio_next, prio_prev) ; 
		     t != &PERCPU_GET(fprr_state)->priorities[i].runnable ;
		     t = FIRST_LIST(t, prio_next, prio_prev)) {
			/* TODO: do we care to differentiate if the thread is
			 * currently in the spd, versus previously? */
			if (cos_thd_cntl(COS_THD_INV_SPD, t->id, target, 0) >= 0)
				if (cnt++ == index) return t;
		}
	}
#ifdef DEFERRABLE
	for (t = FIRST_LIST(&PERCPU_GET(fprr_state)->servers, sched_next, sched_prev) ; 
	     t != &PERCPU_GET(fprr_state)->servers ;
	     t = FIRST_LIST(t, sched_next, sched_prev)) {
		if (cos_thd_cntl(COS_THD_INV_SPD, t->id, target, 0) >= 0)
			if (cnt++ == index) return t;
	}
#endif
	return 0;
}

void runqueue_print(void)
{
	struct sched_thd *t;
	int i = 0;
	
	printc("Core %ld: Running threads (thd, prio, ticks):\n", cos_cpuid());
	for (i = 0 ; i < NUM_PRIOS ; i++) {
		for (t = FIRST_LIST(&PERCPU_GET(fprr_state)->priorities[i].runnable, prio_next, prio_prev) ; 
		     t != &PERCPU_GET(fprr_state)->priorities[i].runnable ;
		     t = FIRST_LIST(t, prio_next, prio_prev)) {
			struct sched_accounting *sa = sched_get_accounting(t);
			unsigned long diff = sa->ticks - sa->prev_ticks;

			//if (!(diff || sa->cycles)) continue;
			printc("\t%d, %d, %ld+%ld/%d\n", t->id, i, diff, (unsigned long)sa->cycles, QUANTUM);
			sa->prev_ticks = sa->ticks;
			sa->cycles = 0;
		}
	}
#ifdef DEFERRABLE
	printc("Suspended threads (thd, prio, ticks):\n");
	for (t = FIRST_LIST(&PERCPU_GET(fprr_state)->servers, sched_next, sched_prev) ; 
	     t != &PERCPU_GET(fprr_state)->servers ;
	     t = FIRST_LIST(t, sched_next, sched_prev)) {
		struct sched_accounting *sa = sched_get_accounting(t);
		unsigned long diff = sa->ticks - sa->prev_ticks;
		
		if (!sched_thd_suspended(t)) continue;
		if (diff || sa->cycles) {
			printc("\t%d, %d, %ld+%ld/%d\n", t->id, 
			       sched_get_metric(t)->priority, diff, 
			       (unsigned long)sa->cycles, QUANTUM);
			sa->prev_ticks = sa->ticks;
			sa->cycles = 0;
		}
	}
#endif
	printc("done printing runqueue.\n");
}

void sched_initialization(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		sched_init_thd(&PERCPU_GET(fprr_state)->priorities[i].runnable, 0, THD_FREE);
	}
	PERCPU_GET(fprr_state)->active = 0;
#ifdef DEFERRABLE
	sched_init_thd(&PERCPU_GET(fprr_state)->servers, 0, THD_FREE);
	PERCPU_GET(fprr_state)->ticks = 0;
#endif
	
}
