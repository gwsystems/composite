/*
 * Copyright 2020, Runyu Pan, GWU, panrunyu@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#ifndef TMR_H
#define TMR_H

/***
 * Timer implementation. This assumes both one-shot and periodic timers.
 */

#include <evt.h>
#include <tmrmgr.h>

#include <errno.h>
#define TMR_TRY_AGAIN		EAGAIN
/* cannot allocate due to a lack of memory */
#define TMR_ERR_NOMEM		ENOMEM
/* passed an invalid argument */
#define TMR_ERR_INVAL_ARG	EINVAL

/*
 * This is the timer data-structure. We assume that timers are always used by one process,
 * and they are never shared. We don't do reference counting on this at all. Also, timers
 * are always free-running; thus it does not have any blkpts. All of these blocking features
 * rely on the event manager.
 */
struct tmr {
	tmr_id_t id;
	evt_res_id_t evt_id;
	unsigned int usecs;
	tmr_flags_t flags;
};

/**
 * `tmr_init` initializes a timer data-structure, and creates a new
 * timer with `time` cycles. The 'type' can be  each of maximum size `item_sz`.
 *
 * - @usecs - The time value. Unit is microseconds.
 * - @tmr_flags_t flags  - Requested timer type, periodic or one-shot.
 * - @return  - `0` on success, `-errval` where `errval` is one of the above `CHAN_ERR_*` values.
 */
static inline int
tmr_init(struct tmr *t, unsigned int usecs, tmr_flags_t flags)
{
	tmr_id_t id;
	int ret;
	
	if ((flags != TMR_ONESHOT) && (flags != TMR_PERIODIC)) return -TMR_ERR_INVAL_ARG;
	id = tmrmgr_create(usecs, flags);
	if (id == 0) return -TMR_ERR_NOMEM;
	
	t->id = id;
	t->usecs = usecs;
	t->flags = flags;
	t->evt_id = 0;
	
	return 0;
}

/**
 * 'tmr_teardown' always directly destroys a timer regardless of its usage.
 * Currently unimplemented - teardown 
 */
static inline int
tmr_teardown(struct tmr *t)
{
	return -TMR_ERR_INVAL_ARG;
}

/**
 * 'tmr_start' starts an initialized timer.
 */
static inline int
tmr_start(struct tmr *t)
{
	int ret;
	
	ret = tmrmgr_start(t->id);
	
	if (ret == -1) return -TMR_ERR_INVAL_ARG;
	else if(ret == -2) return TMR_TRY_AGAIN;
	
	return 0;
}

/**
 * 'tmr_stop' stops an started timer.
 */
static inline int
tmr_stop(struct tmr *t)
{
	int ret;

	ret = tmrmgr_stop(t->id);
	
	if (ret == -1) return -TMR_ERR_INVAL_ARG;
	else if(ret == -2) return TMR_TRY_AGAIN;
	
	return 0;
}

/**
 * Add the event resource id into the timer so that when the timer
 * fires, it will trigger the event of the waiting thread. `disassociate`
 * removes the previous association.
 *
 * - @t      - timer to add to the event set
 * - @eid    - the event resource id to be triggered
 * - @return -
 *
 *     - `0` on success
 *     - `-n` for error with `n` being an errno value
 */
static inline int 
tmr_evt_associate(struct tmr *t, evt_res_id_t eid)
{
	int ret;

	if (t->evt_id != 0) return -1;
	t->evt_id = eid;

	ret = tmrmgr_evt_set(t->id, t->evt_id);

	return ret;
}

static inline evt_res_id_t
tmr_evt_associated(struct tmr *t)
{
	if (t->evt_id == 0) {
		t->evt_id = tmrmgr_evt_get(t->id);
	}

	return t->evt_id;
}

static inline int
tmr_evt_disassociate(struct tmr *t)
{
	evt_res_id_t eid = 0;

	if (t->evt_id == 0) return -1;
	tmrmgr_evt_set(t->id, 0);
	t->evt_id = 0;

	return 0;
}

#endif /* TMR_H */
