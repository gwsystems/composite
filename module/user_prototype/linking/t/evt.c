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

#include <evt.h>

extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern int sched_block(spdid_t spdid);

/* Mapping between external events and internal */
struct evt_map {
	void *extern_evt;
	struct evt *e;
	struct evt_map *next, *prev;
};

struct evt_map evt_map;
struct evt_grp grps;
cos_lock_t evt_lock;

/* 
 * mapping_* functions are for maintaining mappings between an
 * external event and an event structure 
 */
static struct evt_map *mapping_alloc(void *extern_evt, struct evt *e)
{
	struct evt_map *m = malloc(sizeof(struct evt_map));
	
	if (!m) return NULL;
	m->extern_evt = extern_evt;
	m->e = e;
	return m;
}

static struct evt_map *mapping_find(void *extern_evt)
{
	struct evt_map *m;

	for (m = FIRST_LIST(&evt_map, next, prev) ; m != &evt_map ; m = FIRST_LIST(m, next, prev)) {
		if (m->extern_evt == extern_evt) return m;
	}
	return NULL;
}

static void mapping_free(void *extern_evt)
{
	struct evt_map *m;

	m = mapping_find(extern_evt);
	if (NULL != m) {
		REM_LIST(m, next, prev);
		free(m);
	}
}

static inline int mapping_add(struct evt_map *m)
{
	if (mapping_find(m->extern_evt)) return -1;
	ADD_LIST(&evt_map, m, next, prev);
	return 0;
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
int evt_create(spdid_t spdid, void *extern_evt)
{
	u16_t tid = cos_get_thd_id();
	struct evt_grp *g;
	struct evt *e;
	struct evt_map *m;

	lock_take(&evt_lock);
	/* If the mapping exists, it's event better have the group
	 * associated with this thread. */
	if ((m = mapping_find(extern_evt))) {
		goto err; 	/* shouldn't allow recreation of events */
/* 		g = evt_grp_find(tid); */
/* 		if (!g) goto err; */
/* 		if (m->e->grp != g) goto err; */
/* 		goto done; */
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
	m = mapping_alloc(extern_evt, e);
	mapping_add(m);

	lock_release(&evt_lock);
	return 0;
err:
	lock_release(&evt_lock);
	return -1;
}

void evt_free(spdid_t spdid, void *extern_evt)
{
	struct evt_map *m;
	struct evt *e;

	lock_take(&evt_lock);
	m = mapping_find(extern_evt);
	if (NULL != m) goto done;
	e = m->e;
	__evt_free(e);
	mapping_free(extern_evt);
done:
	lock_release(&evt_lock);
	return;
}

/* Wait on a group of events (like epoll) */
void *evt_grp_wait(spdid_t spdid)
{
	struct evt_grp *g;
	struct evt *e;
	void *extern_evt;

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
	return NULL; 
}

/* Wait for a specific event */
int evt_wait(spdid_t spdid, void *extern_evt)
{
	struct evt *e;
	struct evt_map *m;

	while (1) {
		int ret;

		lock_take(&evt_lock);
		m = mapping_find(extern_evt);
		if (NULL == m) goto err;
		e = m->e;
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

int evt_trigger(spdid_t spdid, void *extern_evt)
{
	struct evt_map *m;
	struct evt *e;
	int ret;

	lock_take(&evt_lock);
	m = mapping_find(extern_evt);
	if (NULL == m) goto err;
	e = m->e;
	/* Trigger an event being waited for? */
	if (0 != (ret = __evt_trigger(m->e))) {
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
	INIT_LIST(&evt_map, next, prev);
	INIT_LIST(&grps, next, prev);
	sched_block(cos_spd_id());
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int first = 1;

	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		if (first) {
			init_evts();
			first = 0;
		} else {
			print("event component received too many bootstrap threads. %d%d%d", 0,0,0);
		}
		break;
	default:
		print("evt: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}
	assert(0);
	return;
}
