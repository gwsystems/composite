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

#define TIMER_NO_EVENTS 0ULL

/* Scheduler functions: */
extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
extern int sched_block(spdid_t spdid);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern int sched_timeout_thd(spdid_t spdid);
extern void sched_timeout(spdid_t spdid, unsigned long amnt);
extern unsigned int sched_tick_freq(void);
extern unsigned long sched_timestamp(void);

/* Lets save some typing... */
#define TAKE(spdid) 	if (sched_component_take(spdid)) return -1;
#define RELEASE(spdid)	if (sched_component_release(spdid)) return -1;

/* Monotonically increasing timer counting clicks */
typedef unsigned long long event_time_t;
static volatile event_time_t ticks = 0;

struct thread_event {
	event_time_t event_expiration;
	unsigned short int thread_id, timed_out;
	struct thread_event *next, *prev;
};

static struct thread_event events;
//#define USEC_PER_SEC 1000000
//static unsigned int usec_per_tick = 0;

/* 
 * Return 1 if the inserted event is closer in the future than any
 * others, 0 otherwise.
 */
static int insert_event(struct thread_event *te)
{
	struct thread_event *tmp;
	
	assert(NULL != te);
	assert(te->event_expiration);
	assert(EMPTY_LIST(te, next, prev));
	for (tmp = FIRST_LIST(&events, next, prev) ;
	     ; 			/* condition built into body (see break;) */
	     tmp = FIRST_LIST(tmp, next, prev)) {
		struct thread_event *prev_te = LAST_LIST(tmp, next, prev);
		/* We found our place in the list OR end of list.
		 * Either way, insert before this position */
		if (tmp->event_expiration > te->event_expiration ||
		    &events == tmp) {
			ADD_LIST(prev_te, te, next, prev);
			break;
		}
	}
	if (EMPTY_LIST(&events, next, prev)) {
		ADD_LIST(&events, te, next, prev);
	} 
	assert(!EMPTY_LIST(&events, next, prev));
	assert(!EMPTY_LIST(te, next, prev));
	if (FIRST_LIST(&events, next, prev) == te) return 1;
	return 0;
}

static struct thread_event *find_remove_event(unsigned short int thdid)
{
	struct thread_event *tmp;

	for (tmp = FIRST_LIST(&events, next, prev);
	     tmp != &events;
	     tmp = FIRST_LIST(tmp, next, prev)) {
		if (tmp->thread_id == thdid) {
			REM_LIST(tmp, next, prev);
			return tmp;
		}
	}
	return NULL;
}

/* 
 * This should only be called from the event thread (which has the
 * highest priority), so we don't need to be preempted before waking
 * all threads.
 */
static void event_expiration(event_time_t time)
{
	struct thread_event *tmp, *next;
	spdid_t spdid = cos_spd_id();

	assert(TIMER_NO_EVENTS != time);

	for (tmp = FIRST_LIST(&events, next, prev) ;
	     tmp != &events && tmp->event_expiration <= time ; 
	     tmp = next) {
		assert(tmp);
		next = FIRST_LIST(tmp, next, prev);
		tmp->timed_out = 1;
		sched_wakeup(spdid, tmp->thread_id);
		REM_LIST(tmp, next, prev);
		/* We don't have to deallocate the thread_events as
		 * they are stack allocated on the sleeping
		 * threads. */
	}

	return;
}

static inline event_time_t next_event_time(void)
{
	if (EMPTY_LIST(&events, next, prev)) return TIMER_NO_EVENTS;

	return FIRST_LIST(&events, next, prev)->event_expiration;
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
	struct thread_event te;
	int block_time, ret;

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
	
	INIT_LIST(&te, next, prev);
	te.thread_id = cos_get_thd_id();
	te.timed_out = 0;

	TAKE(spdid);
	ticks = sched_timestamp();
	te.event_expiration = ticks + amnt;
	block_time = ticks;
	assert(te.event_expiration > ticks);
	ret = insert_event(&te);
	RELEASE(spdid);

	if (ret) sched_timeout(spdid, amnt);
	if (-1 == sched_block(spdid)) {
		prints("fprr: sched block failed in timed_event_block.");
	}
	if (te.timed_out) {
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

static void start_timer_thread(void)
{
	spdid_t spdid = cos_spd_id();
	unsigned int tick_freq;

	INIT_LIST(&events, next, prev);
	sched_timeout_thd(spdid);
	tick_freq = sched_tick_freq();
	assert(tick_freq == 100);
	ticks = sched_timestamp();
	/* currently timeouts are expressed in ticks, so we don't need this */
//	usec_per_tick = USEC_PER_SEC/tick_freq;

	/* When the system boots, we have no pending waits */
	assert(EMPTY_LIST(&events, next, prev));
	sched_block(spdid);
	/* Wait for events, then act on expired events.  Loop. */
	while (1) {
		event_time_t next_wakeup;

		ticks = sched_timestamp();
		if (sched_component_take(spdid)) {
			prints("fprr: scheduler lock failed!!!");
		}
		event_expiration(ticks);
		next_wakeup = next_event_time();

		/* Are there no pending events??? */
		if (TIMER_NO_EVENTS == next_wakeup) {
			if (sched_component_release(spdid)) {
				prints("fprr: scheduler lock release failed!!!");
			}
			sched_block(spdid);
		} else {
			unsigned int wakeup;

			assert(next_wakeup > ticks);
			wakeup = (unsigned int)(next_wakeup - ticks);
			if (sched_component_release(spdid)) {
				prints("fprr: scheduler lock release failed!!!");
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
			print("timed_event component received too many bootstrap threads. %d%d%d", 0,0,0);
		}
		break;
	default:
		print("wf_text: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}
	assert(0);
	return;
}
