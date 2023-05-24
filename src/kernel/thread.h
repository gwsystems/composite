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
#include <cos_types.h>
#include <cos_error.h>
#include <chal_regs.h>

#include <types.h>
#include <component.h>
#include <state.h>

struct invstk_entry {
	struct component_ref component;
	uword_t              ip;
	uword_t              sp;
};

struct invstk {
	uword_t             head;
	struct invstk_entry entries[COS_INVSTK_SIZE];
};

struct thd_evt {
	pageref_t       next;
	pageref_t       prev;
	cos_cycles_t    execution;
	uword_t         evt_count;  /* number of event triggers */
	cos_thd_state_t state;	    /* one of THD_STATE_* */
	struct weak_ref dependency; /* only valid if state == THD_STATE_DEPENDENCY */
};

struct thread {
	struct invstk    invstk;
	thdid_t          id;

	pageref_t        sched_thd;
	id_token_t       sched_id;
	sync_token_t     sync_token;
	struct thd_evt   evt;	        /* Event for the thread/scheduler */

	struct regs      regs;
};

COS_FORCE_INLINE struct regs *
thread_switch(struct thread *t, struct regs *rs, uword_t success_retval) {
	struct state_percore *g = state();
	struct thread *curr = g->active_thread;
	struct component_ref *comp;
	cos_retval_t r;

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

        /*
	 * If the current registers are from a system call, set return
         * values accordingly.
	 */
	if (likely(rs->state == REG_STATE_SYSCALL)) {
		regs_retval(rs, REGS_RETVAL_BASE, COS_RET_SUCCESS);
		regs_retval(rs, REGS_RETVAL_BASE + 1, success_retval);
	}
	curr->regs = *rs;

	/* Return the registers we want to restore */
	return &t->regs;
err:
	if (rs->state == REG_STATE_SYSCALL) {
		regs_retval(rs, REGS_RETVAL_BASE, r);
	}

	return rs;
}

struct regs *thread_slowpath(struct thread *t, cos_op_bitmap_t requested_op, struct regs *rs);
