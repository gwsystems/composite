#include "chal_regs.h"
#include "consts.h"
#include "types.h"
#include <cos_consts.h>
#include <cos_error.h>
#include <resources.h>
#include <thread.h>
#include <state.h>

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
	return s->evt.next != s->this;
}

static struct regs *
thread_sched_returnvals(struct thread *s, struct regs *rs)
{
	struct thread *evt_thd;
	struct thd_evt *evt;

	if (!thread_evt_pending(s)) {
		regs_retval(rs, REGS_RETVAL_BASE + 0, COS_RET_SUCCESS);
		regs_retval(rs, REGS_RETVAL_BASE + 1, COS_THD_STATE_NULL);

		return rs;
	}

	evt_thd = thread_evt_dequeue(s);
	evt     = &evt_thd->evt;

	regs_retval(rs, REGS_RETVAL_BASE + 0, COS_RET_SUCCESS);
	regs_retval(rs, REGS_RETVAL_BASE + 1, evt_thd->state);
	regs_retval(rs, REGS_RETVAL_BASE + 2, evt_thd->sched_id);
	regs_retval(rs, REGS_RETVAL_BASE + 3, evt->execution);
	regs_retval(rs, REGS_RETVAL_BASE + 4, 0); /* dependency placeholder */

	return rs;
}

/**
 * `thread_await_evt_
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
	struct thread *evt_thd;
	struct thd_evt *evt;

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
	*thd = (struct thread) {
		.id = id,
		.invstk = (struct invstk) {
			.head = 0, /* We're in the top entry */
			.entries = {
				(struct invstk_entry) { /* Top entry is the current component */
					.component = *compref,
					.ip = 0,
					.sp = 0,
				},
			},
		},
		.this = thisref,
		.state = COS_THD_STATE_EXECUTING,
		.sched_thd = schedthd_ref,
		.sched_id = sched_tok,
		.sync_token = 0,
		.evt = (struct thd_evt) {
			.next = thisref,
			.prev = thisref,
			.execution = 0,
			.dependency = { 0 },
		},
		.regs = { 0 },
	};
	thd->regs.state = REG_STATE_SYSCALL;
	regs_prepare_upcall(&thd->regs, entry_ip, coreid(), id, 0);
}

/**
 * `thread_slowpath` is the aggregate logic for all of the thread
 * capability options. Not all of them necessarily make sense
 * together, but they shouldn't threaten any access control
 * invariants, nor the kernel integrity, so let user-level do as it
 * will. Note here that the permission checking has already been
 * performed to validate that only allowed operations are requested.
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
	struct state_percore *g = state();
	struct thread *curr = g->active_thread;

	if (rs->state == REG_STATE_SYSCALL) {
		regs_retval(rs, REGS_RETVAL_BASE, COS_RET_SUCCESS);
	}

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

	regs_retval(rs, REGS_RETVAL_BASE, -COS_ERR_NO_OPERATION);

	return rs;
}

COS_NEVER_INLINE struct regs *
thread_interrupt_activation(struct thread *t, struct regs *rs)
{
	struct state_percore *g = state();
	struct thread *curr = g->active_thread;

	return thread_trigger_evt(curr, t, rs);
}


COS_NEVER_INLINE struct regs *
thread_timer_activation(struct thread *t, struct regs *rs)
{
	struct state_percore *g = state();
	struct thread *curr = g->active_thread;
	struct thread *sched = g->sched_thread;

	/*
	 * TODO: what if the scheduler is the current thread? How do
	 * we find the scheduler? How do we update the timer?
	 */
	return thread_switch(sched, rs);
}
