/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT

#include <cos_synchronization.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <print.h>
#include <cos_map.h>
#include <cos_vect.h>

#include <errno.h>

#include <evt.h>

extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern int sched_block(spdid_t spdid);

/* A mapping between event ids and actual events */
COS_VECT_CREATE_STATIC(evt_map);
cos_lock_t evt_lock;

struct evt_grp grps;

/* 
 * mapping_* functions are for maintaining mappings between an
 * external event and an event structure 
 */
static int mapping_create(long extern_evt, struct evt *e)
{
	if (extern_evt != cos_vect_add_id(&evt_map, e, extern_evt)) return -1;
	return 0;
}

static inline struct evt *mapping_find(long extern_evt)
{
	return (struct evt*)cos_vect_lookup(&evt_map, extern_evt);
}

static void mapping_free(long extern_evt)
{
	if (cos_vect_del(&evt_map, extern_evt)) assert(0);
}

/* 
 * evt_grp_* functions maintain a mapping between an "event group" and
 * a set of events.  In our case, we are assuming that an event group
 * is essentially a thread.  Thus a thread can wait for a set of
 * events defined by it's "event group".
 */
static inline struct evt_grp *evt_grp_create(spdid_t spdid, u16_t tid)
{
	struct evt_grp *g = malloc(sizeof(struct evt_grp));

	if (!g) return NULL;
	evt_grp_init(g, spdid, tid);
	return g;
}

static inline void evt_grp_free(struct evt_grp *g)
{
	if (!EMPTY_LIST(g, next, prev)) {
		REM_LIST(g, next, prev);
	}
	while (!EMPTY_LIST(&g->events, next, prev)) {
		struct evt *e;
		
		e = FIRST_LIST(&g->events, next, prev);
		REM_LIST(e, next, prev);
	}
	while (!EMPTY_LIST(&g->triggered, next, prev)) {
		struct evt *e;
		
		e = FIRST_LIST(&g->triggered, next, prev);
		REM_LIST(e, next, prev);
	}
	free(g);
}

static inline struct evt_grp *evt_grp_find(u16_t tid)
{
	struct evt_grp *g;

	for (g = FIRST_LIST(&grps, next, prev) ; g != &grps ; g = FIRST_LIST(g, next, prev)) {
		if (g->tid == tid) return g;
	}
	return NULL;
}

static inline int evt_grp_add(struct evt_grp *g)
{
	ADD_LIST(&grps, g, next, prev);
	return 0;
}

/* 
 * FIXME: keeping the lock during a bunch of memory allocation.  This
 * is never good, but the code is much simpler for it.  A trade-off
 * I'm commonly making now.
 */
int evt_create(spdid_t spdid, long extern_evt)
{
	u16_t tid = cos_get_thd_id();
	struct evt_grp *g;
	struct evt *e;
	int ret = -ENOMEM;

	lock_take(&evt_lock);
	/* If the mapping exists, it's event better have the group
	 * associated with this thread. */
	if (NULL != mapping_find(extern_evt)) {
		ret = -EEXIST;
		goto err; 	/* shouldn't allow recreation of events */
	}
	g = evt_grp_find(tid);
	/* If the group associated with this thread hasn't been
	 * created yet. */
	if (!g) {
		g = evt_grp_create(spdid, tid);
		if (NULL == g) goto err;
		e = __evt_new(g);
		if (NULL == e) {
			evt_grp_free(g);
			goto err;
		}
		evt_grp_add(g);
	} else {
		e = __evt_new(g);
		if (NULL == e) goto err;
	}
	e->extern_id = extern_evt;
	if (mapping_create(extern_evt, e)) goto err;

	lock_release(&evt_lock);

	return 0;
err:
	lock_release(&evt_lock);
	return ret;
}

void evt_free(spdid_t spdid, long extern_evt)
{
	struct evt *e;

	lock_take(&evt_lock);
	e = mapping_find(extern_evt);
	if (NULL == e) goto done;
	__evt_free(e);
	mapping_free(extern_evt);
done:
	lock_release(&evt_lock);
	return;
}

/* Wait on a group of events (like epoll) */
long evt_grp_wait(spdid_t spdid)
{
	struct evt_grp *g;
	struct evt *e;
	long extern_evt;

//	printc("evt_grp_wait");

	while (1) {
		lock_take(&evt_lock);

		g = evt_grp_find(cos_get_thd_id());
		if (NULL == g) goto err;
		if (__evt_grp_read(g, &e)) goto err;

		if (NULL != e) {
			extern_evt = e->extern_id;
			lock_release(&evt_lock);
			return extern_evt;
		} else {
			lock_release(&evt_lock);
			if (0 > sched_block(cos_spd_id())) assert(0);
		}
	}
err:
	lock_release(&evt_lock);
	return -1; 
}

/* Wait for a specific event */
int evt_wait(spdid_t spdid, long extern_evt)
{
	struct evt *e;

	while (1) {
		int ret;

		lock_take(&evt_lock);
		e = mapping_find(extern_evt);
		if (NULL == e) goto err;
		if (0 > (ret = __evt_read(e))) goto err;
		lock_release(&evt_lock);
		if (1 == ret) {
			assert(extern_evt == e->extern_id);
			return 0;
		} else {
			if (0 > sched_block(cos_spd_id())) assert(0);
		}
	}
err:
	lock_release(&evt_lock);
	return -1; 
}

int evt_trigger(spdid_t spdid, long extern_evt)
{
	struct evt *e;
	int ret;

	lock_take(&evt_lock);
	e = mapping_find(extern_evt);
//	printc("evt_trigger for %ld: %p", extern_evt, e);
	if (NULL == e) goto err;
	/* Trigger an event being waited for? */
	if (0 != (ret = __evt_trigger(e))) {
		lock_release(&evt_lock);
		if(sched_wakeup(cos_spd_id(), ret)) assert(0);
		return 0;
	}
	lock_release(&evt_lock);
	return 0;
err:
	lock_release(&evt_lock);
	return -1;
}

static void init_evts(void)
{
	lock_static_init(&evt_lock);
	cos_vect_init_static(&evt_map);
	INIT_LIST(&grps, next, prev);
	sched_block(cos_spd_id());
}

void cos_init(void *arg)
{
	volatile static int first = 1;

	if (first) {
		first = 0;
		init_evts();
		assert(0);
	} else {
		prints("net: not expecting more than one bootstrap.");
	}
}
