#pragma once

/**
 * `thread_switch` is the main function to activate a thread, and
 * save the context for the previous thread. Updates the global data
 * given the newly active thread, and switches component context.
 *
 * Note that this should work, though with some inefficiency, if `t`
 * is the current thread.
 *
 * If we cannot switch to the target thread as it's component isn't
 * alive, then an error is returned and `ret_rs` is not modified.
 *
 * - `@t` - the thread to dispatch
 * - `@rs` - current active registers
 * - `@success_retval` - a second retval on successful return for the
 *   current thread. This is only used if the current register set
 *   corresponds to a system call.
 * - `@return` - the registers to activate on return
 */
#include <chal_consts.h>
#include <chal_regs.h>
#include <chal_timer.h>

#include <cos_types.h>
#include <cos_error.h>

#include <compiler.h>
#include <types.h>
#include <component.h>
#include <state.h>
#include <consts.h>

struct invstk_entry {
	struct component_ref component;
	uword_t              ip;
	uword_t              sp;
};

struct invstk {
	uword_t             head;
	struct invstk_entry entries[COS_INVSTK_SIZE];
};

/*
 * Thread events include:
 *
 * 1. a simple counter of asynchronous activations to report the
 *    number of such activations,
 * 2. events that are sent to the scheduler thread which form a
 *    circular doubly linked list and track execution cycles, and
 * 3. synchronous rendezvous-based IPC events that create dependencies
 *    between client and server threads.
 */
struct thd_evt {
	pageref_t       next;
	pageref_t       prev;
	cos_cycles_t    execution;
	uword_t         evt_count;  /* number of event triggers */
	struct weak_ref dependency; /* only valid if state == THD_STATE_DEPENDENCY */
};

/**
 * The `thread` kernel resource, which abstracts all control flow.
 * Retyping operations create threads, and they execute through
 * components, make invocations between them, interact through events
 * and IPC, and schedule/are scheduled. A component is created in a
 * component at the head of its invocation stack. Thread
 * migration-based invocations to other components push entries onto
 * the stack that track the component in which the thread is
 * executing. Thus, the component referenced by the head of the
 * invocation stack indicates the component in which the thread is
 * executing.
 *
 * The active scheduler will always be the one referenced by
 * `sched_thd`. Thus when we pass time values with which to program
 * the timer, this is the scheduler to be notified. In contrast, when
 * a thread awaits an event, or has an event triggered, the
 * `sched_evt_thread` is the one that is notified. For all normal
 * threads, `sched_thd == sched_evt_thread`, and they both reference
 * the scheduler for that thread. However, in the case of a scheduler
 * thread itself, `sched_thd == this`, and `sched_evt_thread` is the
 * parent scheduler thread. The final case is the case of the "boot
 * thread" on a core, in which case, `sched_thd == sched_evt_thread ==
 * this`.
 *
 * Thread invariants:
 *
 * - `state == COS_THD_STATE_EXECUTING` -> `regs.state == REG_STATE_PREEMPTED`
 * - `state == COS_THD_STATE_EVT_AWAITING` -> `evt.evt_count == 0`
 * - `state == COS_THD_STATE_IPC_DEPENDENCY` <-> `valid(evt.dependency)`
 * - `ref2page_ptr(t->this) == t`
 * - `ref2page_ptr(t->sched_thd) != t`
 * - `t == ref2page_ptr(ref2page_ptr(t->evt.next)->evt.prev)`
 * - `t == ref2page_ptr(ref2page_ptr(t->evt.prev)->evt.next)`
 * - `is_thd(evt.next) && is_thd(evt.prev) && is_thd(sched_thd)`
 */
struct thread {
	/* Thread state meta-data */
	struct invstk    invstk;
	thdid_t          id;	        /* our thread id */
	cos_thd_state_t  state;         /* one of THD_STATE_* */
	pageref_t        this;	        /* our own pageref */

	/* Scheduling meta-data */
	pageref_t        sched_thd;     /* our scheduler, or us if we're a scheduler */
	cos_prio_t       priority;      /* lower values = higher priority */
	id_token_t       sched_id;      /* our id, given by, and to be returned to, the scheduler */
	sync_token_t     sync_token;    /* scheduling synchronization token to detect schedule to dispatch races */
	cos_time_t       timeout;	/* If we're a scheduler, this is the timeout we're asking for */
	struct thd_evt   evt;	        /* Event for the thread/scheduler */

	/* Scheduler hierarchy meta-data */
	pageref_t        sched_evt_thd; /* the scheduler to send scheduling events */

	/* Thread registers */
	struct regs      regs;
	struct fpu_regs  fpregs;
};

COS_STATIC_ASSERT(sizeof(struct thread) <= COS_PAGE_SIZE, "Thread structure larger than a page.");

void thread_calculate_returnvals(struct thread *t);

/**
 * `thread_switch` simply switches to the specified thread.
 *
 * This can be called from interrupt and/or syscall contexts, which
 * means that `regs->state` can be either `REG_STATE_SYSCALL` or
 * `REG_STATE_PREEMPTED`. This means we have to be careful about
 * updating registers.
 *
 * - `@t` - The thread to switch to
 * - `@rs` - The current register set
 * - `@return` - The registers to load on returning to user-level
 */
COS_FORCE_INLINE struct regs *
thread_switch(struct thread *t, struct regs *rs) {
	struct state_percore *g    = state();
	struct thread        *curr = g->active_thread;
	struct component_ref *comp;
	cos_retval_t          r;

        /*
	 * This gets written back first, so that the thread's record
         * matches that of the global data. This must be first so that
         * the following operations work if `t == prev`.
	 */
	curr->invstk.head = g->invstk_head;

	/* Switch into the thread's component, if it is alive. */
	comp = &t->invstk.entries[t->invstk.head].component;
        COS_CHECK_THROW(component_activate(comp), r, err);

	/* We're now committed to switching: update the global state */
        g->active_thread = t;
	g->invstk_head   = t->invstk.head;
	g->active_captbl = comp->captbl;

	/* Save the current registers */
	if (likely(&curr->regs != rs)) curr->regs = *rs;

	/*
	 * Formulate return values in a manner consistent with the
	 * thread's state. If the thread is awaiting a asnd trigger
	 * (e.g. from an interrupt), the return values are setup
	 * different from the case where the thread is awaiting
	 * scheduler events.
	 */
	if (likely(t->regs.state == REG_STATE_SYSCALL)) {
		thread_calculate_returnvals(t);
	}

	/* Any thread we're about to execution should be in this state... */
	t->state = COS_THD_STATE_EXECUTING;

	/* Return the registers we want to restore */
	return &t->regs;
err:
	if (rs->state == REG_STATE_SYSCALL) {
		regs_retval(rs, REGS_RETVAL_BASE, r);
	}

	return rs;
}

/**
 * `thread_scheduler_update` sets thread priority, checks
 * synchronization via the `sched_token`, and sets the next timeout,
 * only if the operation includes `COS_OP_THD_SCHEDULE`. To enable
 * later options to proceed, we mask out the schedule operation. Note
 * that the timeout is only programmed if the synchronization token
 * indicates no race.
 *
 * This assumes that the arguments for the synchronization token,
 * timeout, and priority are in the first three general argument
 * registers.
 *
 * - `@ops` - The operations to perform during this thread operation.
 *   This is a pointer so that we can *remove* the schedule flag from
 *   it.
 * - `@t` - Thread we're switching to
 * - `@rs` - Register set for the current system call.
 * - `@return` - Normal return value, notable error on synchronization
 *   race
 */
COS_FORCE_INLINE cos_retval_t
thread_scheduler_update(cos_op_bitmap_t *ops, struct thread *t, struct regs *rs)
{
	struct state_percore *g = state();
	struct thread *s = (struct thread *)ref2page_ptr(t->sched_thd);
	sync_token_t tok = regs_arg(rs, REGS_GEN_ARGS_BASE + 0);
	cos_time_t timeout = regs_arg(rs, REGS_GEN_ARGS_BASE + 1);

	if (!(*ops & COS_OP_THD_SCHEDULE)) return COS_RET_SUCCESS;
	*ops &= ~COS_OP_THD_SCHEDULE;

	t->priority   = regs_arg(rs, REGS_GEN_ARGS_BASE + 2);
	if (tok <= s->sync_token) return -COS_ERR_SCHED_RETRY;
	s->sync_token = tok;
	if (g->timeout != timeout) chal_timer_program(timeout);

	return COS_RET_SUCCESS;
}

struct regs *thread_slowpath(struct thread *t, cos_op_bitmap_t requested_op, struct regs *rs);
void         thread_initialize(struct thread *thd, thdid_t id, id_token_t sched_tok, vaddr_t entry_ip,
                               struct component_ref *compref, pageref_t schedthd_ref, pageref_t thisref);
