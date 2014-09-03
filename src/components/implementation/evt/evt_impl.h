/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef EVT_H
#define EVT_H

#include <cos_component.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <cos_alloc.h>

/* 
 * Structures for an edge-triggered event component.
 * 
 * Current implementation has 64 events per event group.  Threads wait
 * on event groups, and events are triggered/reported giving a group
 * id and event id.  Events cannot be shared across groups.  The sum
 * of these limitations exist only to simplify implementation, and can
 * be relaxed.
 */

typedef enum {
	EVT_TRIGGERED, 	/* has the event been triggered */
	EVT_BLOCKED,    /* the thread is blocked only on this one event */
	EVT_INACTIVE	/* event has not been triggered since last read */
} evt_status_t;

typedef enum {
	EVTG_INACTIVE,
	EVTG_BLOCKED
} evt_grp_status_t;

#define EVT_NUM_PRIOS 2
#define EVT_PER_GRP 250
struct evt_grp;

struct evt {
	evt_status_t status;
	int prio;
	long extern_id;
	int n_wait;
	cpuid_t core_id;
	volatile int n_received; 
	struct evt_grp *grp;
	struct evt *next, *prev;
};

struct evt_grp {
	spdid_t spdid; 		/* currently ignored */
	long tid;               /* thread that waits for events */
	evt_grp_status_t status;
	struct evt_grp *next, *prev;
	struct evt events, triggered[EVT_NUM_PRIOS];
};

static inline void evt_grp_init(struct evt_grp *eg, spdid_t spdid, u16_t tid)
{
	int i;

	eg->spdid = spdid;
	eg->tid = tid;
	eg->status = EVTG_INACTIVE;
	INIT_LIST(eg, next, prev);
	INIT_LIST(&eg->events, next, prev);
	for (i = 0 ; i < EVT_NUM_PRIOS ; i++) {
		INIT_LIST(&eg->triggered[i], next, prev);
	}
}

/* 
 * Return the thread id of the thread that is waiting on the event in
 * the tid argument, or 0 if there is none in the tid argument.
 * Return 0 if no error, -1 otherwise.
 *
 * Some sort of a lock should probably be taken before calling this
 * fn.
 */
static inline int __evt_trigger(struct evt *e)
{
	struct evt_grp *g;
	evt_status_t s;
	evt_grp_status_t gs;

	assert(NULL != e);
	
	/* FIXME: need atomic instruction. */
	e->n_received++;
	assert(e->n_received <= e->n_wait);
	if (e->n_received < e->n_wait) return 0;

	g = e->grp;
	assert(g);
	gs = g->status;
	s = e->status;
	REM_LIST(e, next, prev);
	/* Add to the triggered list */
	ADD_END_LIST(&g->triggered[e->prio], e, next, prev);

	/* mark the event as triggered. */
	e->status = EVT_TRIGGERED;
	g->status = EVTG_INACTIVE;
	/* Someone waiting on this event? */
	if (EVT_BLOCKED == s || EVTG_BLOCKED == gs) {
		return g->tid;
	}
	return 0;
}

/* 
 * As below but don't change the data-structure to reflect that we've
 * blocked if there are no events. 
 */
static int __evt_grp_read_noblock(struct evt_grp *g, struct evt **evt)
{
	int i;
	struct evt *e;
	*evt = NULL;

	assert(NULL != g && NULL != (void*)evt);
	if (cos_get_thd_id() != g->tid) return -1;

	for (i = 0 ; i < EVT_NUM_PRIOS ; i++) {
		if (!EMPTY_LIST(&g->triggered[i], next, prev)) {
			e = FIRST_LIST(&g->triggered[i], next, prev);
			assert(e != &g->triggered[i]);
			REM_LIST(e, next, prev);
			assert(e->status == EVT_TRIGGERED);
			e->status = EVT_INACTIVE;
			g->status = EVTG_INACTIVE;
			*evt = e;
			ADD_LIST(&g->events, e, next, prev);
			
			return 0;
		}
	}
	return 0;
}

/* 
 * return value indicates if an error has occured (-1) or not (0).
 * evt is set to an event if that event has been triggered, or is set
 * to NULL otherwise.  
 *
 * Some sort of a lock should probably be taken before calling this fn.
 */
static int __evt_grp_read(struct evt_grp *g, struct evt **evt)
{
	int ret;

	*evt = NULL;
	ret = __evt_grp_read_noblock(g, evt);
	if (ret) return ret;
	if (NULL == *evt) g->status = EVTG_BLOCKED;
	return 0;
}

static int __evt_read(struct evt *e)
{
	struct evt_grp *g;

	assert(NULL != e);
	assert(e->status != EVT_BLOCKED);
	g = e->grp;
	assert(NULL != g && g->status != EVTG_BLOCKED);
	if (cos_get_thd_id() != g->tid) return -1;
	if (EVT_TRIGGERED == e->status) {
		/* remove from the triggered list */
		REM_LIST(e, next, prev);
		ADD_LIST(&g->events, e, next, prev);
		e->status = EVT_INACTIVE;
		return 1;
	}
	e->status = EVT_BLOCKED;
	return 0;
}

static inline void __evt_free(struct evt *e)
{
	e->grp = NULL;
	e->status = EVT_INACTIVE;
	REM_LIST(e, next, prev);
	free(e);
}

static inline struct evt *__evt_new(struct evt_grp *g)
{
	struct evt *e;

	e = malloc(sizeof(struct evt));
	if (NULL == e) return NULL;
	e->status = EVT_INACTIVE;
	e->prio = 0;
	e->grp = g;
	ADD_LIST(&g->events, e, next, prev);
	return e;
}

#endif /* EVT_H */

