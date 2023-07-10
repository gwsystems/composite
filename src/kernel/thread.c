#include "chal_regs.h"
#include "consts.h"
#include "types.h"
#include <cos_consts.h>
#include <cos_error.h>
#include <resources.h>
#include <thread.h>
#include <state.h>


/*
 * A scheduler's event queue contains the *threads*, one per event,
 * that have triggered events for that scheduler. The events include:
 *
 * - Being triggered by an asynchronous event, including interrupts
 * - Awaiting an asynchronous event
 * - Executing for a number of cycles (after a trigger-driven switch)
 * - IPC-based interactions (including inter-thread dependencies)
 *
 * Assumptions:
 *
 * - Each thread represents a node in a circular, doubly-linked list
 *   of events.
 * - Each thread referenced in the list is through a page reference.
 * - Each thread in the list is alive (they remove themself when being
 *   retyped).
 */

static void
thread_evt_remove(struct thread *t)
{
	struct thread *n, *p;

	p = (struct thread *)ref2page_ptr(t->evt.prev);
	n = (struct thread *)ref2page_ptr(t->evt.next);

	n->evt.prev = p->this;
	p->evt.next = n->this;

	t->evt.next = t->evt.prev = t->this;
}

static struct thread *
thread_evt_dequeue(struct thread *s)
{
	struct thread *n;

	n = (struct thread *)ref2page_ptr(s->evt.next);
	thread_evt_remove(n);

	return n;
}

static void
thread_evt_enqueue(struct thread *s, struct thread *t)
{
	struct thread *p, *n;

	/* If the thread is already in the queue, move it to the head. */
	thread_evt_remove(t);

	n = s;			/* circular list where the new node's next is the scheduler */
	p = (struct thread *)ref2page_ptr(s->evt.prev);

	t->evt.next = n->this;
	t->evt.prev = p->this;

	n->evt.prev = t->this;
	p->evt.next = t->this;

	return;
}

static inline int
thread_evt_pending(struct thread *s)
{
	/* There are no events *also* if s->evt.prev != s->this */
	return s->evt.next != s->this;
}

/**
 * `thread_sched_returnvals` sets up the proper return values for a
 * scheduler thread that is returning from an await on scheduler
 * events.
 *
 * Assumes:
 * - `s` is returning from a `sched_await_or_switch` operation.
 * - Thus, the thread's registers are not in a preempted state.
 *
 * - `@s` - the scheduler thread
 * - `@rs` - current register set of that thread
 * - `@return` - registers to load on return
 */
static struct regs *
thread_sched_returnvals(struct thread *s, struct regs *rs)
{
	struct thread *evt_thd;
	struct thd_evt *evt;

	/*
	 * We might return to the scheduler when there are no events
	 * either because 1. a direct switch to the scheduler is made
	 * (i.e. as the scheduler has the scheduler's critical section
	 * for a core), or 2. a timer interrupt fires, and no events
	 * exist at the time of it firing.
	 */
	if (!thread_evt_pending(s)) {
		regs_retval(rs, REGS_RETVAL_BASE + 0, COS_RET_SUCCESS);
		regs_retval(rs, REGS_RETVAL_BASE + 1, COS_THD_STATE_NULL);

		return rs;
	}

	/* If there are events, return one! */
	evt_thd = thread_evt_dequeue(s);
	evt     = &evt_thd->evt;

	regs_retval(rs, REGS_RETVAL_BASE + 0, COS_RET_SUCCESS);
	/* Are there even more events to process? */
	regs_retval(rs, REGS_RETVAL_BASE + 1, thread_evt_pending(s));
	regs_retval(rs, REGS_RETVAL_BASE + 2, evt_thd->state);
	regs_retval(rs, REGS_RETVAL_BASE + 3, evt_thd->sched_id);
	regs_retval(rs, REGS_RETVAL_BASE + 4, evt->execution);
	regs_retval(rs, REGS_RETVAL_BASE + 5, 0); /* dependency placeholder */

	/* TODO: return 2 events at a time */

	return rs;
}

/**
 * `thread_await_evt_returnvals` sets up the proper return values for
 * a thread that is returning from await_evt calls.
 *
 * Assumes:
 * - `t` is returning from an await_evt operation.
 * - Thus, the thread's registers are not in a preempted state.
 *
 * - `@t` - thread for which we're setting up the return values
 * - `@rs` - current register set of that thread
 * - `@return` - registers to load on return
 */
static struct regs *
thread_await_evt_returnvals(struct thread *t, struct regs *rs)
{
	uword_t cnt = t->evt.evt_count;

	t->evt.evt_count = 0;
	regs_retval(rs, REGS_RETVAL_BASE + 0, COS_RET_SUCCESS);
	regs_retval(rs, REGS_RETVAL_BASE + 1, cnt); /* # of returned events */

	return rs;
}

/**
 * `thread_retrieve_sched_evt_or_switch` A scheduler thread can
 * retrieve an event, or, if there are none, switch to a target
 * thread. We assume that the current thread (`curr`) is a scheduler.
 *
 * - `@curr` - the current, scheduler thread
 * - `@t` - the thread to switch to if there are no events
 * - `@rs` - the current thread's register set
 * - `@return` - the register set to activate
 */
struct regs *
thread_retrieve_sched_evt_or_switch(struct thread *curr, struct thread *t, struct regs *rs)
{
	/* If there are no events, simply switch to the target thread */
	if (thread_evt_pending(curr)) {
		return thread_sched_returnvals(curr, rs);
	}

	curr->state = COS_THD_STATE_SCHED_AWAIT;

	return thread_switch(t, rs);
}

/**
 * `thread_await_evt` is called by a thread that waits to wait for an
 * asynchronous activation event. If there are no pending events, we
 * switch to the scheduler after passing it the await event.
 *
 * - `@curr` - the current thread awaiting asynchronous activation
 * - `@rs` - the thread's register set
 * - `@return` - the register set to return into
 */
struct regs *
thread_await_evt(struct thread *curr, struct regs *rs)
{
	struct thread *sched = (struct thread *)ref2page_ptr(curr->sched_thd);

	/* Pending events? return them! */
	if (curr->evt.evt_count > 0) {
		return thread_await_evt_returnvals(curr, rs);
	}

	/* Add this thread into the event queue for the scheduler... */
	curr->state = COS_THD_STATE_EVT_AWAITING;
	thread_evt_enqueue(sched, curr);
	/* ...and switch to the scheduler. */

	return thread_switch(sched, rs);
}

/**
 * `thread_trigger_evt` triggers an asynchronous event in thread `t`
 * from the current thread `curr`. If `t` is awaiting that event,
 * activate it. Switch to it if it has a higher priority, and,
 * regardless, send the scheduler an event for the activation, and
 * increment the event count for the target.
 *
 * Note that this can be called from both system calls and from
 * interrupts, thus `regs->state` can be either `REG_STATE_SYSCALL` or
 * `REG_STATE_PREEMPTED`. This means we have to be careful about
 * updating registers.
 *
 * - `@curr` - the current thread performing the activation.
 * - `@t` - the thread being activated.
 * - `@rs` - the current register set (for `curr`).
 * - `@return` - the registers to activate.
 */
struct regs *
thread_trigger_evt(struct thread *curr, struct thread *t, struct regs *rs)
{
	struct thread *s = (struct thread *)ref2page_ptr(t->sched_thd);

	/* Increment the event count for the target thread. */
	t->evt.evt_count++;

	/* Should we wake up an awaiting thread? */
	if (t->state != COS_THD_STATE_EVT_AWAITING) return rs;

	/* tell the scheduler that the thread is now activate */
	t->state = COS_THD_STATE_EVT_TRIGGERED;
	thread_evt_enqueue(s, t);
	/* Does the target thread have higher priority? Switch to it! */
	if (t->priority >= curr->priority) return rs;

	return thread_switch(t, rs);
}

/**
 * `thread_calculate_returnvals` sets the return values for the
 * thread, `t`, that we're switching to. This requires understanding
 * the thread's state, and populating its return values according to
 * expected layouts.
 *
 * Assume: `t->regs.state == REG_STATE_SYSCALL`, thus the thread is
 * *expecting* properly formatted return values.
 *
 * - `@t` - thread that we're switching to.
 */
void
thread_calculate_returnvals(struct thread *t)
{
	/* thread has cooperatively dispatched */
	if (likely(t->state == COS_THD_STATE_EXECUTING)) {
		regs_retval(&t->regs, REGS_RETVAL_BASE + 0, COS_RET_SUCCESS);
		return;
	}
	switch (t->state) {
	case COS_THD_STATE_EVT_TRIGGERED:
	case COS_THD_STATE_EVT_AWAITING: /* thread is awaiting asynchronous event */
		thread_await_evt_returnvals(t, &t->regs);
		return;
	case COS_THD_STATE_SCHED_AWAIT:  /* thread is a scheduler, awaiting event */
		thread_sched_returnvals(t, &t->regs);
		return;
	}
}

void
thread_initialize(struct thread *thd, thdid_t id, id_token_t sched_tok, vaddr_t entry_ip, struct component_ref *compref, pageref_t schedthd_ref, pageref_t thisref)
{
	struct thread *sched = (struct thread *)ref2page_ptr(schedthd_ref);

	*thd = (struct thread) {
		.id = id,
		.invstk = (struct invstk) {
			.head = 0, /* We're in the top entry */
			.entries = {
				/* The top invstk entry is the current component */
				(struct invstk_entry) {
					.component = *compref,
					.ip = 0,
					.sp = 0,
				},
			},
		},
		.this = thisref,
		.state = COS_THD_STATE_EXECUTING,
		.sched_thd = schedthd_ref,
		.sched_evt_thd = schedthd_ref,
		.priority = COS_PRIO_LOW,
		.sched_id = sched_tok,
		.sync_token = 0,
		.evt = (struct thd_evt) {
			.next = thisref,
			.prev = thisref,
			.execution = 0,
			.dependency = { 0 },
		},
	};

	/* Make sure that the scheduler thread properly understands its role. */
	sched->sched_thd = schedthd_ref;

	thd->regs.state = REG_STATE_SYSCALL;
	regs_prepare_upcall(&thd->regs, entry_ip, coreid(), id, 0);
}

/**
 * `thread_slowpath` is the aggregate logic for all of the thread
 * capability operations.
 *
 * We assume this is only called from syscalls (thus `rs->state ==
 * REG_STATE_SYSCALL`).
 *
 * We assume here that the registers (arguments) are laid out statically:
 *
 * - `2` - scheduling token
 * - `3` - thread capability for a tcap
 * - `4` - cycles to transfer to the tcap
 * - `5` - absolute cycles to use to program the timer
 *
 * Recall that `0` is the capability id, and `1` is the operations
 * bitmap.
 */
COS_NEVER_INLINE struct regs *
thread_slowpath(struct thread *t, cos_op_bitmap_t requested_op, struct regs *rs)
{
	struct thread *curr = state()->active_thread;
	cos_retval_t r = -COS_ERR_NO_OPERATION;

	/* Default return value for the current thread */
	regs_retval(rs, REGS_RETVAL_BASE, COS_RET_SUCCESS);
	COS_CHECK_THROW(thread_scheduler_update(&requested_op, t, rs), r, err);

	switch(requested_op) {
	case COS_OP_THD_DISPATCH:
		return thread_switch(t, rs);
	case COS_OP_THD_TRIGGER_EVT:
		return thread_trigger_evt(curr, t, rs);
	case COS_OP_THD_EVT_OR_DISPATCH:
		return thread_retrieve_sched_evt_or_switch(curr, t, rs);
	case COS_OP_THD_AWAIT_EVT:
		return thread_await_evt(curr, rs);
	}
err:
	regs_retval(rs, REGS_RETVAL_BASE, r);

	return rs;
}

/**
 * `thread_interrupt_activation` should be called in interrupt context
 * to trigger an asynchronous event for the target thread, `t`. This
 * is essentially the exact same logic as for normal user-level
 * accessible triggers. This might continue executing in the current,
 * active thread, or might switch to the interrupt thread.
 *
 * - `@t` - thread to trigger: the "interrupt thread"
 * - `@rs` - the current register set (of the active thread)
 * - `@return` - the registers we should activate on return to user-level.
 */
struct regs *
thread_interrupt_activation(struct thread *t, struct regs *rs)
{
	struct thread *curr = state()->active_thread;

	return thread_trigger_evt(curr, t, rs);
}

/**
 * `thread_timer_activation` is called by the timer interrupt to
 * active the corresponding scheduler. This function relies on the
 * logic of the `thread_switch` to properly set up the scheduler's
 * return values. This *unconditionally* switches to the scheduler
 * thread as at this point, the "next" timer isn't programmed.
 *
 * Assumes: there's effectively only a single scheduler in the system.
 * Will later get TCaps working to sanely enable multiple schedulers.
 *
 * - `@rs` - the current register set
 * - `@return` - the register set of the scheduler to activate
 */
struct regs *
thread_timer_activation(struct regs *rs)
{
	struct thread *sched = state()->sched_thread;

	if (unlikely(!sched)) return rs;

	return thread_switch(sched, rs);
}
