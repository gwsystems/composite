#include "thread.h"

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
	struct globals_percore *g = state();
	struct thread *curr = g->active_thread;

	if (rs->state == REG_STATE_SYSCALL) {
		regs_retval(rs, REGS_RETVAL_BASE, COS_RET_SUCCESS);
	}

	if (requested_op & COS_OP_THD_TIME_TRANSFER) {
		/* Transfer time between tcaps */
	}
	if (requested_op & COS_OP_THD_TIMER_PROGRAM) {
		/* Program the timer */
	}
	if (requested_op & COS_OP_THD_AWAIT_EVENT) {
		/*
		 * Only makes sense if `t` is the current thread. If
		 * there are events (normal events, or scheduler
		 * events), return them, and potentially switch to the
		 * scheduler (updating `t` to be the scheduler, and
		 * updating the ops to ignore priority). If we're
		 * switching to a scheduler, we need to be sure to
		 * load its registers with the proper event
		 * information.
		 */
	}
	if (requested_op & COS_OP_THD_TRIGGER_EVENT) {
		/*
		 * Activate an event for a thread, attempting to
		 * switch to it, and increment the event count.
		 */
	}
	if (!(requested_op & COS_OP_THD_IGNORE_PRIO)) {
		/*
		 * Check tcap priority to determine if we should
		 * switch threads. If not, return without changing
		 * threads.
		 */
	}
        /*
         * Some of the previous ops had conditions whereby we return.
         * At this point, we haven't escaped the function yet, so
         * check if we're switching threads.
	 */
	if (requested_op == COS_OP_THD_DISPATCH) {
		return thread_switch(t, rs, 0);
	}


	return rs;
}
