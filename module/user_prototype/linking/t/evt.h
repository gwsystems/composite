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
	EVT_INACTIVE,	/* event has not been triggered since last read */
	EVT_FREE
} evt_status_t;

typedef enum {
	EVTG_INACTIVE,
	EVTG_BLOCKED
} evt_grp_status_t;

#define EVT_PER_GRP 64
struct evt_grp;

struct evt {
	evt_status_t status;
	long extern_id;
	struct evt_grp *grp;
};

struct evt_grp {
	spdid_t spdid; 		/* currently ignored */
	u16_t tid;              /* thread that waits for events */
	evt_grp_status_t status;
	struct evt_grp *next, *prev;
	struct evt evts[EVT_PER_GRP];
};

static inline void evt_grp_init(struct evt_grp *eg, spdid_t spdid, u16_t tid)
{
	int i;
	
	eg->spdid = spdid;
	eg->tid = tid;
	eg->status = EVTG_INACTIVE;
	INIT_LIST(eg, next, prev);
	for (i = 0; i < EVT_PER_GRP ; i++) {
		struct evt *e = &eg->evts[i];

		e->status = EVT_FREE;
		e->grp = eg;
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

	assert(NULL != e);
	g = e->grp;
	s = e->status;
	/* mark the event as triggered. */
	e->status = EVT_TRIGGERED;
	/* Someone waiting on this event? */
	if (EVT_BLOCKED == s || EVTG_BLOCKED == s) {
		return g->tid;
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
	struct evt *e;
	int i;
	*evt = NULL;

	assert(NULL != g && NULL != (void*)evt);
	if (cos_get_thd_id() != g->tid) return -1;

	for (i = 0; i < EVT_PER_GRP ; i++) {
		e = &g->evts[i];
		if (e->status == EVT_FREE) continue;
		if (e->status == EVT_TRIGGERED) {
			e->status = EVT_INACTIVE;
			g->status = EVTG_INACTIVE;
			*evt = e;
			return 0;
		}
	}
	g->status = EVTG_BLOCKED;
	return 0;
}

static int __evt_read(struct evt *e)
{
	struct evt_grp *g;

	assert(NULL != e);
	assert(e->status != EVT_FREE && e->status != EVT_BLOCKED);
	g = e->grp;
	assert(NULL != g);
	if (cos_get_thd_id() != g->tid) return -1;
	if (EVT_TRIGGERED == e->status) {
		e->status = EVT_INACTIVE;
		return 1;
	}
	e->status = EVT_BLOCKED;
	return 0;
}

static inline void __evt_free(struct evt *e)
{
	e->status = EVT_FREE;
}

static inline struct evt *__evt_new(struct evt_grp *g)
{
	int i;

	for (i = 0; i < EVT_PER_GRP ; i++) {
		struct evt *e = &g->evts[i];
		
		if (EVT_FREE == e->status) {
			e->status = EVT_INACTIVE;
			return e;
		}
	}
	return NULL;
}

#endif /* EVT_H */

