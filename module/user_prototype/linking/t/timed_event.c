/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_time.h>

#define TIMER_NO_EVENTS 0ULL

/* Scheduler functions: */
extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
extern int sched_block(spdid_t spdid);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern int sched_timeout_thd(spdid_t spdid);
extern void sched_timeout(spdid_t spdid, unsigned long amnt);
extern unsigned int sched_tick_freq(void);

#define TAKE(spdid) 	if (sched_component_take(spdid)) return -1;
#define RELEASE(spdid)	if (sched_component_release(spdid)) return -1;

/* Monotonically increasing timer counting clicks */
typedef unsigned long long event_time_t;
static volatile event_time_t ticks = 0;

struct thread_event {
	event_time_t event_expiration;
	unsigned short int thread_id, timed_out;
	struct thread_event *next;
};

static struct thread_event *events = NULL;
#define USEC_PER_SEC 1000000
static unsigned int usec_per_tick = 0;

/* 
 * Return 1 if the inserted event is closer in the future than any
 * others, 0 otherwise.
 */
static int insert_event(struct thread_event *te)
{
	struct thread_event *tmp, *prev;

	assert(te);
	if (NULL == events) {
		events = te;
		te->next = NULL;
		return 1;
	}
	if (events->event_expiration > te->event_expiration) {
		te->next = events;
		events = te;
		return 1;
	}

	prev = events;
	tmp = events->next;
	while (tmp) {
		if (tmp->event_expiration > te->event_expiration) {
			break;
		}

		prev = tmp;
		tmp = tmp->next;
	}
	te->next = tmp;
	prev->next = te;

	return 0;
}

#ifdef NIL
// FIXME: double check linked list manips
static void remove_event(struct thread_event *te)
{
	struct thread_event *tmp, *prev;

	assert(te);

	if (NULL == events) {
		return;
	}
	if (te == events) {
		events = events->next;
		return;
	}
	
	prev = events;
	tmp = events->next;
	while (tmp) {
		/* Event present?  Remove it! */
		if (tmp == te) {
			prev->next = tmp->next;
			return;
		} 
		/* Event not present? */
		if (tmp->event_expiration > te->event_expiration) {
			return;
		}

		prev = tmp;
		tmp = tmp->next;
	}

	return;
}
#endif

static struct thread_event *find_remove_event(unsigned short int thdid)
{
	struct thread_event *prev = events, *tmp;

	if (!prev) return NULL;
	tmp = prev->next;
	if (prev->thread_id == thdid) {
		events = tmp;
		return prev;
	}

	while (tmp) {
		if (tmp->thread_id == thdid) {
			prev->next = tmp->next;
			return tmp;
		}

		prev = tmp;
		tmp = tmp->next;
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
	struct thread_event *tmp = events;
	spdid_t spdid = cos_spd_id();

//	if (NULL == events) assert(TIMER_NO_EVENTS == time);
	if (NULL == events || TIMER_NO_EVENTS == time) return;

	for (; tmp && tmp->event_expiration <= time ; tmp = tmp->next) {
		tmp->timed_out = 1;
		sched_wakeup(spdid, tmp->thread_id);
		/* We don't have to deallocate the thread_events as
		 * they are stack allocated on the sleeping
		 * threads. */
	}
	/* Remove events from list */
	events = tmp;
}

static inline event_time_t next_event_time(void)
{
	if (!events) return TIMER_NO_EVENTS;

	return events->event_expiration;
}

/**
 * FIXME: store the spdid blocking thread is invoking from, and make
 * sure that the wakeup request comes from the same component
 */

static event_time_t next_wakeup = TIMER_NO_EVENTS;

/* Return the amount of time we slept */
int timed_event_block(spdid_t spdinv, unsigned int amnt)
{
	spdid_t spdid = cos_spd_id();
	struct thread_event te;
	int block_time, ret;

	assert(0 != amnt);

	/* 
	 * Convert from usec to ticks
	 *
	 * +2 here as we don't know how far through the current clock
	 * tick we are _and_ we don't know how far into the clock tick
	 * the wakeup time is.  The sleep is supposed to be for _at
	 * least_ amnt clock ticks, thus here we are conservative.
	 */
	amnt = (amnt/(unsigned int)usec_per_tick) + 2;
	
	te.thread_id = cos_get_thd_id();
	te.next = NULL;
	te.timed_out = 0;

	TAKE(spdid);
	te.event_expiration = ticks + amnt;
	block_time = ticks;

	ret = insert_event(&te);
	next_wakeup = next_event_time();
	assert(TIMER_NO_EVENTS != next_wakeup);
	RELEASE(spdid);
	if (ret) {
		sched_timeout(spdid, amnt);
	}
	assert(-1 != sched_block(spdid));

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
	return ((int)ticks - block_time - 1)*usec_per_tick;
}

int timed_event_wakeup(spdid_t spdinv, unsigned short int thd_id)
{
	spdid_t spdid = cos_spd_id();
	struct thread_event *evt;

	TAKE(spdid);
	if (NULL == (evt = find_remove_event(thd_id))) {
		RELEASE(spdid);
		return 1;
	}
	RELEASE(spdid);

	return sched_wakeup(spdid, thd_id);
}

static void start_timer_thread(void)
{
	spdid_t spdid = cos_spd_id();
	unsigned int tick_freq;

	sched_timeout_thd(spdid);
	tick_freq = sched_tick_freq();
	assert(tick_freq == 100);
	usec_per_tick = USEC_PER_SEC/tick_freq;

	/* When the system boots, we have no pending waits */
	assert(NULL == events);
	sched_block(spdid);
	/* Wait for events, then act on expired events.  Loop. */
	while (1) {
		assert(!sched_component_take(spdid));
		event_expiration(next_wakeup);
		/* Error here: probably didn't release when you should have */
		if (TIMER_NO_EVENTS != next_wakeup) ticks = next_wakeup;
		next_wakeup = next_event_time();

		/* Are there any pending events??? */
		if (TIMER_NO_EVENTS == next_wakeup) {
			assert(!sched_component_release(spdid));
			//prints("Timer thread about to block.");
			sched_block(spdid);
			//prints("Timer thread waking up.");
		} else {
			unsigned int wakeup;

			assert(next_wakeup > ticks);
			wakeup = (unsigned int)(next_wakeup - ticks);
			assert(!sched_component_release(spdid));

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

	return;
}
