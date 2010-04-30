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

/* Lets save some typing... */
#define TAKE(spdid) 	if (sched_component_take(spdid)) return -1;
#define RELEASE(spdid)	if (sched_component_release(spdid)) return -1;

/* Monotonically increasing timer counting clicks */
typedef unsigned long long event_time_t;
static volatile event_time_t ticks = 0;
const event_time_t TIMER_NO_EVENTS = ~0;

#define TE_TIMED_OUT 0x1
#define TE_BLOCKED   0x2
#define TE_PERIODIC  0x4

struct thread_event {
	event_time_t event_expiration;
	unsigned int period, dl_missed; /* missed deadlines */
	unsigned short int thread_id, flags;
	struct thread_event *next, *prev;
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
			if (!b) tmp->dl_missed++;
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
	if (te->flags & TE_TIMED_OUT) {
		return TIMER_EXPIRED;
	}
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
	if (!te->flags & TE_PERIODIC) goto err;
		
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

	TAKE(spdid);
	te = te_pget(tid);
	if (NULL == te) BUG();
	if (!te->flags & TE_PERIODIC) goto err;
		
	assert(!EMPTY_LIST(te, next, prev));
	te->flags |= TE_BLOCKED;
	RELEASE(spdid);

	if (-1 == sched_block(spdid, 0)) {
		prints("fprr: sched block failed in timed_event_periodic_wait.");
	}

	return 0;
err:
	RELEASE(spdid);
	return -1;
}

static void start_timer_thread(void)
{
	spdid_t spdid = cos_spd_id();
	unsigned int tick_freq;

	INIT_LIST(&events, next, prev);
	events.thread_id = 0;
	INIT_LIST(&periodic, next, prev);
	periodic.thread_id = 0;

	cos_vect_init_static(&thd_evts);
	cos_vect_init_static(&thd_periodic);

	sched_timeout_thd(spdid);
	tick_freq = sched_tick_freq();
	assert(tick_freq == 100);
	ticks = sched_timestamp();
	/* currently timeouts are expressed in ticks, so we don't need this */
//	usec_per_tick = USEC_PER_SEC/tick_freq;

	/* When the system boots, we have no pending waits */
	assert(EMPTY_LIST(&events, next, prev));
	sched_block(spdid, 0);
	/* Wait for events, then act on expired events.  Loop. */
	while (1) {
		event_time_t next_wakeup;

		cos_mpd_update(); /* update mpd config given this
				   * thread is now in this component
				   * (no dependency if we are in the
				   * same protection domain as the
				   * scheduler) */
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

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int first = 1;

	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		if (first) {
			start_timer_thread();
			first = 0;
		} else {
			printc("timed_event component received too many bootstrap threads.");
		}
		break;
	default:
		printc("wf_text: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		BUG();
		return;
	}
	BUG();
	return;
}
