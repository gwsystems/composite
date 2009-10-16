/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2008.
 */

#define COS_FMT_PRINT

#include <cos_synchronization.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <print.h>
#include <cos_map.h>
//#include <cos_vect.h>

#include <errno.h>

#include <evt.h>

//#define ACT_LOG
#ifdef ACT_LOG
#define ACT_LOG_LEN 32
#define ACTION_TIMESTAMP 1

typedef enum {
	ACT_TRIGGER,
	ACT_WAKEUP,
	ACT_SLEEP,
	ACT_WAIT,
	ACT_WAIT_GRP
} action_t;
typedef enum {
	ACT_SPDID,
	ACT_EVT_ID,
	ACT_T1,
	ACT_T2,
	ACT_ITEM_MAX
} action_item_t;
#define NUM_ACT_ITEMS ACT_ITEM_MAX
#include <cos_actlog.h>
#define ACT_RECORD(a, s, e, t1, t2)					\
	do {								\
		unsigned long as[] = {s, e, t1, t2};			\
		action_record(a, as, NULL);				\
	} while (0)
#else
#define ACT_RECORD(a, s, e, t1, t2)
#endif

extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern int sched_block(spdid_t spdid, unsigned short int thd_dep);

/* A mapping between event ids and actual events */
COS_MAP_CREATE_STATIC(evt_map);
cos_lock_t evt_lock;

struct evt_grp grps;

/* 
 * mapping_* functions are for maintaining mappings between an
 * external event and an event structure 
 */
static long mapping_create(struct evt *e)
{
	return cos_map_add(&evt_map, e);
}

static inline struct evt *mapping_find(long extern_evt)
{
	struct evt *e = cos_map_lookup(&evt_map, extern_evt);
	if (NULL == e) return e;
	assert(e->extern_id == extern_evt);
	return e;
}

static void mapping_free(long extern_evt)
{
	if (cos_map_del(&evt_map, extern_evt)) assert(0);
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
	int i;

	if (!EMPTY_LIST(g, next, prev)) {
		REM_LIST(g, next, prev);
	}
	while (!EMPTY_LIST(&g->events, next, prev)) {
		struct evt *e;
		
		e = FIRST_LIST(&g->events, next, prev);
		REM_LIST(e, next, prev);
	}
	for (i = 0 ; i < EVT_NUM_PRIOS ; i++) {
		while (!EMPTY_LIST(&g->triggered[i], next, prev)) {
			struct evt *e;
			
			e = FIRST_LIST(&g->triggered[i], next, prev);
			REM_LIST(e, next, prev);
		}
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
long evt_create(spdid_t spdid)
{
	u16_t tid = cos_get_thd_id();
	struct evt_grp *g;
	struct evt *e;
	int ret = -ENOMEM;

	lock_take(&evt_lock);
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
	e->extern_id = mapping_create(e);
	if (0 > e->extern_id) goto free_evt_err;

	lock_release(&evt_lock);

	return e->extern_id;
free_evt_err:
	__evt_free(e);
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
	struct evt *e = NULL;
	long extern_evt;

	while (1) {
		lock_take(&evt_lock);

		g = evt_grp_find(cos_get_thd_id());
		ACT_RECORD(ACT_WAIT_GRP, spdid, e ? e->extern_id : 0, cos_get_thd_id(), 0);
		if (NULL == g) goto err;
		if (__evt_grp_read(g, &e)) goto err;

		if (NULL != e) {
			extern_evt = e->extern_id;
			lock_release(&evt_lock);
			return extern_evt;
		} else {
			lock_release(&evt_lock);
			ACT_RECORD(ACT_SLEEP, spdid, 0, cos_get_thd_id(), 0);
			if (0 > sched_block(cos_spd_id(), 0)) assert(0);
		}
	}
err:
	lock_release(&evt_lock);
	return -1; 
}

/* As above, but return more than one event notifications */
int evt_grp_mult_wait(spdid_t spdid, struct cos_array *data)
{
	struct evt_grp *g;
	struct evt *e = NULL;
	int evt_gathered = 0, evt_max;

	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	evt_max = data->sz / sizeof(long);

	while (1) {
		lock_take(&evt_lock);

		g = evt_grp_find(cos_get_thd_id());
		ACT_RECORD(ACT_WAIT_GRP, spdid, e ? e->extern_id : 0, cos_get_thd_id(), 0);
		if (NULL == g) goto err;

		/* gather multiple events */
		do {
			if (__evt_grp_read_noblock(g, &e)) goto err;
			if (NULL != e) {
				((long*)data->mem)[evt_gathered] = e->extern_id;
				evt_gathered++;
			}
		} while (e && evt_gathered < evt_max);

		/* return them if they were gathered */
		if (evt_gathered > 0) {
			lock_release(&evt_lock);
			return evt_gathered;
		}

		/* 
		 * otherwise sleep till there is an event (first we
		 * need to call evt_grp_read to set the blocked
		 * status)
		 */
		if (__evt_grp_read(g, &e)) goto err;
		assert(NULL == e);
		lock_release(&evt_lock);
		ACT_RECORD(ACT_SLEEP, spdid, 0, cos_get_thd_id(), 0);
		if (0 > sched_block(cos_spd_id(), 0)) assert(0);
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
		ACT_RECORD(ACT_WAIT, spdid, e->extern_id, cos_get_thd_id(), 0);
		lock_release(&evt_lock);
		if (1 == ret) {
			assert(extern_evt == e->extern_id);
			return 0;
		} else {
			ACT_RECORD(ACT_SLEEP, spdid, e->extern_id, cos_get_thd_id(), 0);
			if (0 > sched_block(cos_spd_id(), 0)) assert(0);
		}
	}
err:
	lock_release(&evt_lock);
	return -1; 
}

int evt_trigger(spdid_t spdid, long extern_evt)
{
	struct evt *e;
	int ret = 0;

	lock_take(&evt_lock);

	e = mapping_find(extern_evt);
	if (NULL == e) goto err;

	ACT_RECORD(ACT_TRIGGER, spdid, e->extern_id, cos_get_thd_id(), 0);
	/* Trigger an event being waited for? */
	if (0 != (ret = __evt_trigger(e))) {
		lock_release(&evt_lock);
		ACT_RECORD(ACT_WAKEUP, spdid, e->extern_id, cos_get_thd_id(), ret);
		if (sched_wakeup(cos_spd_id(), ret)) assert(0);
	} else {
		lock_release(&evt_lock);
	}

	return 0;
err:
	lock_release(&evt_lock);
	return -1;
}

int evt_set_prio(spdid_t spdid, long extern_evt, int prio)
{
	struct evt *e;

	if (prio >= EVT_NUM_PRIOS) return -1;

	lock_take(&evt_lock);
	e = mapping_find(extern_evt);
	if (NULL == e) goto err;
	e->prio = prio;
	/* FIXME: place into correct list in the group if it is triggered */
	lock_release(&evt_lock);
	return 0;
err:
	lock_release(&evt_lock);
	return -1;
}

static void init_evts(void)
{
	lock_static_init(&evt_lock);
	cos_map_init_static(&evt_map);
	INIT_LIST(&grps, next, prev);
}

void cos_init(void *arg)
{
	static volatile int first = 1;

	if (first) {
		first = 0;
		init_evts();
	} else {
		prints("net: not expecting more than one bootstrap.");
	}
}

static void bin(void)
{
	sched_block(cos_spd_id(), 0);
}

#ifdef ACT_LOG

unsigned long *evt_stats(spdid_t spdid, unsigned long *stats)
{
	struct action *a;
	int sz = (NUM_ACT_ITEMS + 2) * sizeof(unsigned long);

	if (!cos_argreg_buff_intern((char*)stats, sz)) {
		return NULL;
	}
	
	if (NULL == (a = action_report())) return NULL;
	memcpy(stats, a, sz);
	return stats;
}

int evt_stats_len(spdid_t spdid)
{
	return NUM_ACT_ITEMS + 2;
}

#else

unsigned long *evt_stats(spdid_t spdid, unsigned long *stats) { return NULL; }
int evt_stats_len(spdid_t spdid) { return 0; }

#endif
