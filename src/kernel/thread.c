#include "chal_regs.h"
#include "consts.h"
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
	return s->evt.next != s->evt.prev;
}

/**
 * Assume that the current thread (`curr`) is a scheduler.
 */
struct regs *
thread_retrieve_evt_or_switch(struct thread *curr, struct thread *t, struct regs *rs)
{
	struct thread *evt_thd;
	struct thd_evt *evt;
	cos_thd_state_t state;

	if (!thread_evt_pending(curr)) return thread_switch(t, rs, 0);

	evt_thd = thread_evt_dequeue(curr);
	evt = &evt_thd->evt;
	state = evt_thd->state;

	if (state == COS_THD_STATE_IPC_DEPENDENCY) {
		pageref_t d_ref;

		/*
		 * If the thread we depend on has been destroyed,
		 * return an error, and mark it as executing.
		 *
		 * TODO: This should be replaced with an exception
		 * thread activation.
		 */
		if (resource_weakref_deref(&evt->dependency, &d_ref) != COS_RET_SUCCESS) {
			state = COS_THD_STATE_EXECUTING;
			regs_retval(&evt_thd->regs, REGS_RETVAL_BASE, -COS_ERR_NOT_LIVE);
		} else {
			struct thread *d = (struct thread *)ref2page_ptr(d_ref);

			regs_retval(rs, REGS_RETVAL_BASE + 4, d->sched_id);
		}
	}
	regs_retval(rs, REGS_RETVAL_BASE, 1); /* # of returned events */
	regs_retval(rs, REGS_RETVAL_BASE + 1, evt_thd->sched_id);
	regs_retval(rs, REGS_RETVAL_BASE + 2, state);
	regs_retval(rs, REGS_RETVAL_BASE + 3, evt->execution);

	return rs;
}

struct regs *
thread_await_asnd(struct thread *curr, struct thread *t, struct regs *rs)
{
	t->evt.evt_count++;

	if (t->state == COS_THD_STATE_EVT_AWAITING) {
		t->state = COS_THD_STATE_EXECUTING;
		if (t->regs.state == REG_STATE_SYSCALL) {
			regs_retval(&t->regs, REGS_RETVAL_BASE, COS_RET_SUCCESS); /* # of returned events */
			regs_retval(&t->regs, REGS_RETVAL_BASE + 1, t->evt.evt_count);
			t->evt.evt_count = 0;
		}
	}

	if (t->state != COS_THD_STATE_EXECUTING) {
		regs_retval(rs, REGS_RETVAL_BASE, COS_RET_SUCCESS); /* # of returned events */

		return rs;
	}

	/* FIXME: assuming both threads have the same scheduler. */
	struct thread *sched = (struct thread *)ref2page_ptr(t->sched_thd);



	return COS_RET_SUCCESS;
}

cos_retval_t
thread_trigger_asnd(struct thread *t, struct regs *rs)
{
	struct thread *sched = (struct thread *)ref2page_ptr(t->sched_thd);

	return COS_RET_SUCCESS;
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

	if (requested_op & COS_OP_THD_EVT_OR_DISPATCH) {
		/* scheduler wait for event */
	}
	if (requested_op & COS_OP_THD_AWAIT_ASND) {
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
	if (requested_op & COS_OP_THD_TRIGGER_ASND) {
		/*
		 * Activate an event for a thread, attempting to
		 * switch to it, and increment the event count.
		 */
	}
        /*
         * Some of the previous ops had conditions whereby we return.
         * At this point, we haven't escaped the function yet, so
         * check if we're switching threads.
	 */
	if (requested_op & COS_OP_THD_DISPATCH) {
		return thread_switch(t, rs, 0);
	}


	return rs;
}
