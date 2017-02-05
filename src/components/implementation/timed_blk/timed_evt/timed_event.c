/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2008
 */

#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>
#include <cos_time.h>
#include <cos_list.h>
#include <cos_vect.h>
#include <cos_alloc.h>

#include <timed_blk.h>
#include <periodic_wake.h>

#include <sched.h>

#include <sys/param.h> 		/* MIN/MAX */
#include <res_spec.h> /* For creating timer thread */
/* Lets save some typing... */
#define TAKE(spdid) 	if (sched_component_take(spdid)) return -1;
#define RELEASE(spdid)	if (sched_component_release(spdid)) return -1;

/* Monotonically increasing timer counting clicks */
typedef unsigned long long event_time_t;
static volatile event_time_t ticks = 0;
const event_time_t TIMER_NO_EVENTS = ~0;
unsigned long cyc_per_tick;

#define TE_TIMED_OUT 0x1
#define TE_BLOCKED   0x2
#define TE_PERIODIC  0x4

struct thread_event {
	event_time_t event_expiration;
	unsigned short int thread_id, flags;
	struct thread_event *next, *prev;

	/* if flags & TE_PERIODIC */
	unsigned int period, missed;
	unsigned short int dl, dl_missed, need_restart; /* missed deadlines */
	unsigned int samples, miss_samples;
	long long lateness_tot, miss_lateness_tot;
	long long completion;
};

static struct thread_event events, periodic;

COS_VECT_CREATE_STATIC(thd_evts);
COS_VECT_CREATE_STATIC(thd_periodic);

/* 
 * FIXME: to make this predictable (avoid memory allocation in the
 * must-be-predictable case, we should really cos_vect_add_id when we
 * first find out about the possibility of the thread making any
 * invocations.
 */
static struct thread_event *__te_get(unsigned short int tid, cos_vect_t *v)
{
	struct thread_event *te;

	te = cos_vect_lookup(v, tid);
	if (NULL == te) {
		te = malloc(sizeof(struct thread_event));
		if (NULL == te) return NULL;
		memset(te, 0, sizeof(struct thread_event));
		te->thread_id = tid;
		INIT_LIST(te, next, prev);
		if (tid != cos_vect_add_id(v, te, tid)) return NULL;
	}
	return te;
}

static struct thread_event *te_pget(unsigned short int tid)
{
	return __te_get(tid, &thd_periodic);
}

static struct thread_event *te_get(unsigned short int tid)
{
	return __te_get(tid, &thd_evts);
}

//#define USEC_PER_SEC 1000000
//static unsigned int usec_per_tick = 0;

/* 
 * Return 1 if the inserted event is closer in the future than any
 * others, 0 otherwise.
 */
static int __insert_event(struct thread_event *te, struct thread_event *events)
{
	struct thread_event *tmp;

	assert(NULL != te);
	assert(te->event_expiration);
	assert(EMPTY_LIST(te, next, prev));
	assert(events->next && events->prev);
	if (EMPTY_LIST(events, next, prev)) {
		ADD_LIST(events, te, next, prev);
	} else for (tmp = FIRST_LIST(events, next, prev) ;
		    ; /* condition built into body (see break;) */
		    tmp = FIRST_LIST(tmp, next, prev)) {
		assert(tmp);
		struct thread_event *prev_te = LAST_LIST(tmp, next, prev);
		assert(prev_te);
		assert(tmp->prev && tmp->next);
		/* We found our place in the list OR end of list.
		 * Either way, insert before this position */
		if (tmp->event_expiration > te->event_expiration ||
		    events == tmp) {
			ADD_LIST(prev_te, te, next, prev);
			assert(prev_te->next == te && te->prev == prev_te);
			assert(te->next == tmp && tmp->prev == te);
			break;
		}
		assert(tmp->next && tmp->prev);
	}
	
	assert(!EMPTY_LIST(events, next, prev));
	assert(!EMPTY_LIST(te, next, prev));

	return 0;
}

static int insert_event(struct thread_event *te)
{
	return __insert_event(te, &events);
}

static int insert_pevent(struct thread_event *te)
{
	return __insert_event(te, &periodic);
}

static struct thread_event *find_remove_event(unsigned short int thdid)
{
	struct thread_event *tmp;

	for (tmp = FIRST_LIST(&events, next, prev);
	     tmp != &events;
	     tmp = FIRST_LIST(tmp, next, prev)) {
		if (tmp->thread_id == thdid) {
			REM_LIST(tmp, next, prev);
			assert(events.next && events.prev);
			return tmp;
		}
	}
	return NULL;
}

static void __event_expiration(event_time_t time, struct thread_event *events)
{
	spdid_t spdid = cos_spd_id();

	struct thread_event *tmp, *next_te;

	assert(TIMER_NO_EVENTS != time);

	for (tmp = FIRST_LIST(events, next, prev) ;
	     tmp != events && tmp->event_expiration <= time ; 
	     tmp = next_te) {
		u8_t b;
		unsigned short int tid;

		assert(tmp);
		next_te = FIRST_LIST(tmp, next, prev);
		assert(next_te && next_te->prev == tmp && tmp->next == next_te);
		tmp->flags |= TE_TIMED_OUT;
		REM_LIST(tmp, next, prev);
		b = tmp->flags & TE_BLOCKED;
		tmp->flags &= ~TE_BLOCKED;
		tid = tmp->thread_id;

		if (tmp->flags & TE_PERIODIC) {
			/* thread hasn't blocked? deadline miss! */
			if (!b) {
				tmp->dl_missed++;
				tmp->need_restart++;
				if (!tmp->missed) { /* first miss? */
					tmp->missed = 1;
					if (tmp->completion) {
						/* compute the lateness of 
						   last task finished on time */
						long long t;
						rdtscll(t);
						tmp->lateness_tot += -(t - tmp->completion);
						tmp->samples++;
 					}
					/* save time of deadline, unless we
					 * have saved the time of an earlier
					 * deadline miss */
					rdtscll(tmp->completion);
					tmp->miss_samples++;
					tmp->samples++;
				}
			} else {
				assert(!tmp->missed); /* on time, compute lateness */
				long long t;
				assert (tmp->completion) ;
				rdtscll(t);
				tmp->lateness_tot += -(t - tmp->completion);
				tmp->samples++;
				tmp->completion = 0;
			}

			tmp->dl++;
			/* Next periodic deadline! */
			tmp->event_expiration += tmp->period;
			insert_pevent(tmp);
		}

		if (b) sched_wakeup(spdid, tmp->thread_id);
		/* We don't have to deallocate the thread_events as
		 * they are stack allocated on the sleeping
		 * threads. */
	}
}

/* 
 * This should only be called from the event thread (which has the
 * highest priority), so we don't need to be preempted before waking
 * all threads.
 */
static void event_expiration(event_time_t time)
{
	__event_expiration(time, &periodic);
	__event_expiration(time, &events);

	return;
}

static inline event_time_t next_event_time(void)
{
	event_time_t e = TIMER_NO_EVENTS, p = TIMER_NO_EVENTS;

	e = EMPTY_LIST(&events, next, prev) ? 
		TIMER_NO_EVENTS :
		FIRST_LIST(&events, next, prev)->event_expiration;
	p = EMPTY_LIST(&periodic, next, prev) ? 
		TIMER_NO_EVENTS : 
		FIRST_LIST(&periodic, next, prev)->event_expiration;
	/* assume here that TIMER_NO_EVENTS > all other values */
	return MIN(e, p);
}

/**
 * FIXME: store the spdid blocking thread is invoking from, and make
 * sure that the wakeup request comes from the same component
 */
/*
 * FIXME: allow amnt to be specified in time units rather than ticks.
 */
int timed_event_block(spdid_t spdinv, unsigned int amnt)
{
	spdid_t spdid = cos_spd_id();
	struct thread_event *te;
	int block_time;
	event_time_t t;

	if (amnt == 0) return 0;
	/* 
	 * Convert from usec to ticks
	 *
	 * +2 here as we don't know how far through the current clock
	 * tick we are _and_ we don't know how far into the clock tick
	 * the wakeup time is.  The sleep is supposed to be for _at
	 * least_ amnt clock ticks, thus here we are conservative.
	 */
	//amnt = (amnt/(unsigned int)usec_per_tick) + 2;
	/* update: seems like +1 should be enough */
	amnt++;
	
	TAKE(spdid);
	te = te_get(cos_get_thd_id());
	if (NULL == te) BUG();
	assert(EMPTY_LIST(te, next, prev));

	te->thread_id = cos_get_thd_id();
	te->flags &= ~TE_TIMED_OUT;
	te->flags |= TE_BLOCKED;

	ticks = sched_timestamp();
	te->event_expiration = ticks + amnt;
	block_time = ticks;
   	assert(te->event_expiration > ticks);
	t = next_event_time();
	insert_event(te);
	assert(te->next && te->prev && !EMPTY_LIST(te, next, prev));
	RELEASE(spdid);

	if (t != next_event_time()) sched_timeout(spdid, amnt);
	if (-1 == sched_block(spdid, 0)) {
		prints("fprr: sched block failed in timed_event_block.");
	}

	/* we better have been taking off the list! */
	assert(EMPTY_LIST(te, next, prev));
	if (te->flags & TE_TIMED_OUT) return TIMER_EXPIRED;

	/* 
	 * The event has already been removed from event list in
	 * event_expiration by the timeout thread.
	 * 
	 * Minus 1 here as we must report the amount of time we are
	 * sure we waited for.  As we don't know how far into the tick
	 * we were when we slept, and how far the wakeup is into a
	 * tick, we must account for this.
	 */
	return ((int)ticks - block_time - 1); //*usec_per_tick; /* expressed in ticks currently */
}

int timed_event_wakeup(spdid_t spdinv, unsigned short int thd_id)
{
	spdid_t spdid = cos_spd_id();
	struct thread_event *evt;

	TAKE(spdid);
	ticks = sched_timestamp();
	if (NULL == (evt = find_remove_event(thd_id))) {
		RELEASE(spdid);
		return 1;
	}
	RELEASE(spdid);
	assert(evt->thread_id == thd_id);

	return sched_wakeup(spdid, thd_id);
}


static long te_get_reset_lateness(struct thread_event *te)
{
	long long avg;

	if (0 == te->samples){
		if (!te->missed)
			return 0;	
		else
			te->samples = 1;
	}
	
	if (te->missed && te->completion){
		long long t;
		rdtscll(t);
		te->lateness_tot += (t - te->completion);
	}
	avg = te->lateness_tot / te->samples;
	avg = (avg >> 20) + ! ((avg & 1048575) == 0);/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */
	
	te->lateness_tot = 0;
	te->samples = 0;

	return avg;
}

static long te_get_reset_miss_lateness(struct thread_event *te)
{
	long long avg;

	if (0 == te->miss_samples){
		if (!te->missed)
			return 0;
		else
			te->miss_samples = 1;
	}

	if (te->missed && te->completion){
		long long t;
		rdtscll(t);
		te->miss_lateness_tot += (t - te->completion);
	}

	avg = te->miss_lateness_tot / te->miss_samples;
	avg = (avg >> 20) + ! ((avg & 1048575) == 0);/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */

	te->miss_lateness_tot = 0;
	te->miss_samples = 0;

	return avg;
}

long periodic_wake_get_lateness(unsigned short int tid)
{
	struct thread_event *te;
	spdid_t spdid = cos_spd_id();
	long ret;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) {
		RELEASE(spdid);
		return 0;
	}
	ret = te_get_reset_lateness(te);
	RELEASE(spdid);
	
	return ret;
}

long periodic_wake_get_miss_lateness(unsigned short int tid)
{
	struct thread_event *te;
	spdid_t spdid = cos_spd_id();
	long ret;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) {
		RELEASE(spdid);
		return 0;
	}
	ret = te_get_reset_miss_lateness(te);
	RELEASE(spdid);
	
	return ret;
}

int periodic_wake_get_misses(unsigned short int tid)
{
	struct thread_event *te;
	spdid_t spdid = cos_spd_id();
	int m;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) {
		RELEASE(spdid);
		return -1;
	}
	m = te->dl_missed;
	te->dl_missed = 0;
	RELEASE(spdid);

	return m;
}

int periodic_wake_get_deadlines(unsigned short int tid)
{
	struct thread_event *te;
	spdid_t spdid = cos_spd_id();
	int m;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) {
		RELEASE(spdid);
		return -1;
	}
	m = te->dl;
	te->dl = 0;
	RELEASE(spdid);

	return m;
}

int periodic_wake_get_period(unsigned short int tid)
{
	struct thread_event *te;
	spdid_t spdid = cos_spd_id();
	int p;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) {
		RELEASE(spdid);
		return -1;
	}
	p = (int)te->period;
	RELEASE(spdid);

	return p;
}

int periodic_wake_create(spdid_t spdinv, unsigned int period)
{
	struct thread_event *te;
	unsigned short int tid = cos_get_thd_id();
	spdid_t spdid = cos_spd_id();
	event_time_t n, t;

	if (period < 1) return -1;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (te->flags & TE_PERIODIC) {
		assert(!EMPTY_LIST(te, next, prev));
		REM_LIST(te, next, prev);
	}
	assert(EMPTY_LIST(te, next, prev));
	te->flags |= TE_PERIODIC;
	te->period = period;
	ticks = sched_timestamp();
	te->event_expiration = n = ticks + period;
	assert(n > ticks);

	t = next_event_time();
	assert(t > ticks);
	insert_pevent(te);
	if (t > n) sched_timeout(spdid, n-ticks);
	te->need_restart = 0;

	RELEASE(spdid);

	return 0;
}

int periodic_wake_remove(spdid_t spdinv, unsigned short int tid)
{
	spdid_t spdid = cos_spd_id();
	struct thread_event *te;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) goto err;
		
	assert(!EMPTY_LIST(te, next, prev));
	REM_LIST(te, next, prev);
	te->flags = 0;
	
	RELEASE(spdid);

	return 0;
err:
	RELEASE(spdid);
	return -1;
}

int periodic_wake_wait(spdid_t spdinv)
{
	spdid_t spdid = cos_spd_id();
	struct thread_event *te;
	u16_t tid = cos_get_thd_id();
	long long t;

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!(te->flags & TE_PERIODIC)) goto err;
		
	assert(!EMPTY_LIST(te, next, prev));

	rdtscll(t);

	if (te->missed) {	/* we're late */
		long long diff;
		if (te->miss_samples == 0){
			te->miss_samples = 1;
			te->samples = 1;
		}

		assert(te->completion);

		diff = (t - te->completion);
		te->lateness_tot += diff;
		//te->samples++;
		te->miss_lateness_tot += diff;
		//te->miss_samples++;
		
		te->completion = 0;
		te->missed = 0;
	} else {		/* on time! */
		te->completion = t;
	}
	if (te->need_restart > 0) {
		te->need_restart--;
		RELEASE(spdid);
		return 0;
	}
	te->flags |= TE_BLOCKED;
	RELEASE(spdid);

	if (-1 == sched_block(spdid, 0)) {
		prints("fprr: sched block failed in timed_event_periodic_wait.");	}


	return 0;
err:
	RELEASE(spdid);
	return -1;
}

static void start_timer_thread(void)
{
	spdid_t spdid = cos_spd_id();
	unsigned int tick_freq;

	sched_timeout_thd(spdid);
	tick_freq = sched_tick_freq();
	assert(tick_freq == 100);
	ticks = sched_timestamp();
	/* currently timeouts are expressed in ticks, so we don't need this */
//	usec_per_tick = USEC_PER_SEC/tick_freq;
	cyc_per_tick = sched_cyc_per_tick();
	//	printc("cyc_per_tick = %lld\n", cyc_per_tick);

	/* When the system boots, we have no pending waits */
	assert(EMPTY_LIST(&events, next, prev));
	sched_block(spdid, 0);
	/* Wait for events, then act on expired events.  Loop. */
	while (1) {
		event_time_t next_wakeup;

		ticks = sched_timestamp();
		if (sched_component_take(spdid)) {
			prints("fprr: scheduler lock failed!!!");
			BUG();
		}
		event_expiration(ticks);
		next_wakeup = next_event_time();

		/* Are there no pending events??? */
		if (TIMER_NO_EVENTS == next_wakeup) {
			if (sched_component_release(spdid)) {
				prints("fprr: scheduler lock release failed!!!");
				BUG();
			}

			sched_block(spdid, 0);
		} else {
			unsigned int wakeup;
#ifdef LINUX_HIGHEST_PRIORITY
			//gap assert(next_wakeup > ticks);
#endif
			assert(next_wakeup > ticks);
			wakeup = (unsigned int)(next_wakeup - ticks);
			if (sched_component_release(spdid)) {
				prints("fprr: scheduler lock release failed!!!");
				BUG();
			}
			sched_timeout(spdid, wakeup);
		}
	}
}

void cos_init()
{
	union sched_param sp;
	static int first = 1;

	if (first) {
		first = 0;
		INIT_LIST(&events, next, prev);
		events.thread_id = 0;
		INIT_LIST(&periodic, next, prev);
		periodic.thread_id = 0;

		cos_vect_init_static(&thd_evts);
		cos_vect_init_static(&thd_periodic);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 3;

		if (cos_thd_create(start_timer_thread, NULL, sp.v, 0, 0) <= 0) BUG();
	} else {
		printc("timed_event component received too many bootstrap threads.");
	}
}

