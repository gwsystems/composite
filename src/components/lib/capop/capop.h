/*
 * BSD 3 clause licence
 * Copyright 2023, Gabe Parmer
 */

/***
 * This library contains the wrappers around Composite kernel
 * capability operations. While not strictly true, I'll refer to these
 * below as different "system calls" (this is not true, as there is
 * only one system call: capability activation). They provide no
 * abstraction over the kernel API, thus these are very difficult to
 * use directly. This documentation should detail the groups of system
 * calls, their constraints, and their scalability properties.
 *
 * These are broken into APIs centered around:
 *
 * - Threads,
 * - Capability creation (in capability tables),
 * - Retyping (in page-tables),
 * - Resource table construction, and
 * - Hardware capability operations.
 */

#pragma once

#include "cos_chal_regs.h"
#include "cos_consts.h"
#include <cos_types.h>
#include <cos_error.h>
#include <cos_regs.h>
#include <cos_compiler.h>

COS_FORCE_INLINE static inline cos_retval_t
__capop_4_1_op(cos_cap_t cap, cos_op_bitmap_t op, uword_t a0, uword_t a1, uword_t a2, uword_t a3)
{
	uword_t ret;

	cos_syscall_4_1(cap, op, a0, a1, a2, a3, &ret);

	return ret;
}

COS_FORCE_INLINE static inline cos_retval_t
__capop_9_1_op(cos_cap_t cap, cos_op_bitmap_t op, uword_t a0, uword_t a1, uword_t a2, uword_t a3, uword_t a4, uword_t a5, uword_t a6, uword_t a7, uword_t a8)
{
	uword_t ret;

	cos_syscall_9_1(cap, op, a0, a1, a2, a3, a4, a5, a6, a7, a8, &ret);

	return ret;
}

/***
 * Thread system calls used by schedulers to switch between threads,
 * manage scheduling events, and to enable threads to await and
 * trigger events.
 */

/**
 * `capop_thd_dispatch` is used to switch to a thread, while also
 * programming the timer, setting the thread's priority (used if there
 * is an interrupt to determine if the interrupt thread should be
 * switched to). This function should be used by schedulers, and its
 * intention is to switch to the scheduler thread instead of
 * dispatching, if the scheduling thread has pending events. Note that
 * the synchronization token also enables scheduling code to
 * synchronize with kernel scheduling decisions. If the scheduler
 * thread has pending events, instead this will switch to the
 * scheduler. This function call be called by any thread, not only by
 * scheduling threads. This enables any active thread to switch to
 * another (given the capability-access). The scheduling thread is
 * special only because it serves two purposes: 1. receive timer
 * interrupts (after `timeout`), and 2. receive and process scheduling
 * events.
 *
 * Note that the thread being dispatched to might have cooperatively
 * relinquished the core (e.g. by previously calling this function),
 * or it might be preempted. Thus, this call can involve changing
 * protection domains (e.g. if we switch to a thread that was
 * preempted in another component). See the slite work by Gadepalli et
 * al. that avoids system calls in the case of cooperative yields.
 *
 * Constraints required to avoid an error reply:
 *
 * - The target thread must be bound to this core.
 * - `timeout` is a time in the future (`> cos_now()`)
 *
 * Scalability: Scales perfectly as all resources are core-local.
 *
 * - `@thdcap` - The capability to the thread to switch to.
 * - `@tok` - The scheduler token to use to detect preemptions.
 * - `@timeout` - The absolute time of the next timer interrupt.
 * - `@prio` - The priority of the thread we dispatch to.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_thd_dispatch(cos_cap_t thdcap, sync_token_t tok, cos_time_t timeout, cos_prio_t prio)
{
	return __capop_4_1_op(thdcap, COS_OP_THD_DISPATCH | COS_OP_THD_SCHEDULE, tok, timeout, prio, 0);
}

struct capop_sched_evt {
	int pending;		   /* More events pending after this? */
	cos_thd_state_t thd_state; /* State of the thread */
	id_token_t thd_id_token;   /* The token/cookie to identify the thread */
	cos_cycles_t execution;	   /* How long the thread executed */
};

/**
 * `capop_thd_switch_or_sched_evt` is called by a scheduler thread,
 * and enables it to switch to a desired thread, unless there are
 * pending scheduler events. The arguments mimic those of
 * `capop_thd_dispatch` as the intention is to hopefully simply
 * dispatch to the thread. However, if there are pending scheduler
 * events, the kernel returns an event in the `evt` structure which
 * includes `pending` if there are additional pending events,
 * `thd_state` to report the thread's state, `thd_id_token` which is
 * the token/cookie passed in when the thread was created and will
 * often be a pointer to the thread structure, and finally,
 * `execution` which is the number of cycles executed by the thread.
 *
 * Constraints required to avoid an error reply:
 *
 * - The target thread must be bound to this core.
 * - `timeout` is a time in the future (`> cos_now()`)
 * - The scheduler thread must be bound to the same core as the target.
 *
 * Assumption: the `evt->thd_id_token` is the `id_token_t` passed in
 * when creating the thread. Thus, the scheduler should control this
 * value when the thread is created, and likely pass in a pointer to
 * the thread structure.
 *
 * Scalability: Scales perfectly as all resources are core-local
 * (including the scheduler thread).
 *
 * - `@thdcap` - A capability to the thread to switch to.
 * - `@tok` - The scheduler token to use to detect preemptions.
 * - `@timeout` - The absolute time of the next timer interrupt.
 * - `@prio` - The priority of the thread we dispatch to.
 * - `@evt` - The event structure populated if there are scheduler evts.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_thd_switch_or_sched_evt(cos_cap_t thdcap, sync_token_t tok, cos_time_t timeout, cos_prio_t prio, struct capop_sched_evt *evt)
{
	uword_t ret, pending, thd_state, thd_id_token, execution;

	cos_syscall_9_9(thdcap, COS_OP_THD_EVT_OR_DISPATCH | COS_OP_THD_SCHEDULE, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			&ret, &pending, &thd_state, &thd_id_token, &execution, 0, 0, 0, 0);

	*evt = (struct capop_sched_evt) {
		.pending = pending,
		.thd_state = thd_state,
		.thd_id_token = thd_id_token,
		.execution = execution,
	};

	return ret;
}

/**
 * `capop_thd_evt_trigger` triggers an event for another thread. The
 * assumption is that the other thread is `capop_thd_await`ing to
 * receive the event. This will cause a kernel-based scheduling
 * decision based on the priorities the scheduler has previously given
 * to the threads. As such, it enables asynchronous thread activation
 * based on priorities. Threads that are awoken by the trigger will
 * send an event to the scheduler as such.
 *
 * Note that trigger and await are implemented with IPIs when the
 * triggering thread and awaiting thread are on different cores.
 *
 * Constraints: N/A
 *
 * Scalability: Scales perfectly if the target thread is bound to the
 * same core as the current thread. IPIs are used otherwise, which
 * might cause livelock issues if used gratuitously.
 *
 * - `@thdcap` - The capability to the thread being sent the event.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_thd_evt_trigger(cos_cap_t thdcap)
{
	return __capop_4_1_op(thdcap, COS_OP_THD_TRIGGER_EVT, 0, 0, 0, 0);
}

/**
 * `capop_thd_await` enables a thread to block awaiting an event
 * from another. If events were already triggered, this will return
 * immediately. Otherwise, this will cause the scheduler thread to
 * activate, and receive an event that we're awaiting. Later, when a
 * thread triggers an event for this thread, it will eventually
 * execute. When it returns, the `evt_cnt` is set to the number of
 * times that the event was triggered since we last awaited (due to
 * delays in scheduling, this could be large).
 *
 * Note that trigger and await are implemented with IPIs when the
 * triggering thread and awaiting thread are on different cores.
 *
 * Constraints required to avoid an error reply:
 *
 * - The `thdcap` must target a capability referring to the *current
 *   (calling) thread*.
 *
 * Scalability: Scales perfectly as all resources are core-local
 * (including the scheduler thread).
 *
 * - `@thdcap` - the capability to our *own thread*.
 * - `@evt_cnt` - a count of how many times the event was triggered.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_thd_await(cos_cap_t thdcap, uword_t *evt_cnt)
{
	uword_t ret;

	cos_syscall_4_4(thdcap, COS_OP_THD_AWAIT_EVT, 0, 0, 0, 0, &ret, evt_cnt, 0, 0);

	return ret;
}

/***
 * Capability-table system calls to create, remove, and copy
 * capability slots. All of the retype operations have a shared set of
 * constraints and scalability properties, which follow here.
 *
 * Constraints required to avoid an error reply:
 *
 * - The capability-table slot should be free.
 * - The capability-table slot should be quiesced.
 * - The target page/resource in the page-tables must match the capability type.
 * - The `ops` should be a subset of those of the captbl and pgtbl nodes.
 *
 * Scalability:
 *
 * - We are writing to the capability-table slot to create the
 *   capability reference to the resource. While each capability slot
 *   takes a 64B cache-line, two CAS instructions can attempt to
 *   create the slot on different cores at the same time. This can
 *   cause contention, but since there is no "retry" or spinning, this
 *   is unlikely to cause a measurable scalability issue (besides
 *   having a latency on the order of a cross-core cache-coherency
 *   operation).
 * - Counter-intuitively, this doesn't cause scalability concerns.
 *   While we are creating a capability reference to a resource, all
 *   references from capabilities are weakrefs, thus don't modify
 *   global state outside of the capability table slot.
 */

COS_FORCE_INLINE static inline cos_retval_t
__capop_captbl_cap_create(cos_cap_t ctcap, cos_op_bitmap_t op, cos_cap_type_t type, uword_t ct_off, cos_op_bitmap_t ops,
			  cos_cap_t pt_node, cos_cap_t pt_off, vaddr_t entry, inv_token_t token)
{
	return __capop_9_1_op(ctcap, op, type, ct_off, ops, pt_node, pt_off, entry, token, 0, 0);
}

/**
 * `capop_captbl_cap_create_thd` enables the creation of a
 * capability to a thread in a slot in a last-level capability-table
 * node.
 *
 * - `@ctcap` - The cap-table node in which we'll create the capability.
 * - `@ct_off` - The offset into that node of the capability to-be.
 * - `@ops` - The allowed operations we wish to enable on the thread.
 * - `@pt_node` - The page-table node in which we can find the thread.
 * - `@pt_off` - The offset into that node at which we find the thread.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_create_thd(cos_cap_t ctcap, uword_t ct_off, cos_op_bitmap_t ops, cos_cap_t pt_node, cos_cap_t pt_off)
{
	return __capop_captbl_cap_create(ctcap, COS_OP_CAPTBL_CAP_CREATE_THD, COS_CAP_TYPE_THD, ct_off, ops, pt_node, pt_off, 0, 0);
}

/**
 * `capop_captbl_cap_create_comp` enables the creation of a
 * capability to a component in a slot in a last-level capability-table node.
 *
 * - `@ctcap` - The cap-table node in which we'll create the capability.
 * - `@ct_off` - The offset into that node of the capability to-be.
 * - `@ops` - The allowed operations we wish to enable on the component.
 * - `@pt_node` - The page-table node in which we can find the component.
 * - `@pt_off` - The offset into that node at which we find the component.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_create_comp(cos_cap_t ctcap, uword_t ct_off, cos_op_bitmap_t ops, cos_cap_t pt_node, cos_cap_t pt_off)
{
	return __capop_captbl_cap_create(ctcap, COS_OP_CAPTBL_CAP_CREATE_COMP, COS_CAP_TYPE_COMP, ct_off, ops, pt_node, pt_off, 0, 0);
}

/**
 * `capop_captbl_cap_create_sinv` enables the creation of a
 * capability to a synchronous invocation end-point in a slot in a
 * last-level capability-table node. The `entry_addr` is the
 * instruction pointer to use for execution in the server component,
 * and `token` is the token passed to the server for the invocation.
 *
 * - `@ctcap` - The cap-table node in which we'll create the capability.
 * - `@ct_off` - The offset into that node of the capability to-be.
 * - `@ops` - The allowed operations we wish to enable on the sinv.
 * - `@pt_node` - The page-table node in which we can find the server component.
 * - `@pt_off` - The offset into that node at which we find the server component.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_create_sinv(cos_cap_t ctcap, uword_t ct_off, cos_op_bitmap_t ops, cos_cap_t pt_node, cos_cap_t pt_off, vaddr_t entry_addr, inv_token_t token)
{
	return __capop_captbl_cap_create(ctcap, COS_OP_CAPTBL_CAP_CREATE_SINV, COS_CAP_TYPE_SINV, ct_off, ops, pt_node, pt_off, entry_addr, token);
}

/**
 * `capop_captbl_cap_create_captbl` enables the creation of a
 * capability to a capability table node (`n`) at a specific level in
 * a slot in a last-level capability-table node.
 *
 * - `@ctcap` - The cap-table node in which we'll create the capability.
 * - `@ct_off` - The offset into that node of the capability to-be.
 * - `@level` - The level of the captbl node `n`.
 * - `@ops` - The allowed operations we wish to enable on the captbl.
 * - `@pt_node` - The page-table node in which we can find `n`.
 * - `@pt_off` - The offset into that node at which we find `n`.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_create_captbl(cos_cap_t ctcap, uword_t ct_off, uword_t level, cos_op_bitmap_t ops, cos_cap_t pt_node, cos_cap_t pt_off)
{
	const cos_cap_type_t types[COS_CAPTBL_MAX_DEPTH] = {COS_CAP_TYPE_CAPTBL_0, COS_CAP_TYPE_CAPTBL_1};

	if (unlikely(level >= COS_CAPTBL_MAX_DEPTH)) return -COS_ERR_WRONG_CAP_TYPE;

	return __capop_captbl_cap_create(ctcap, COS_OP_CAPTBL_CAP_CREATE_RESTBL, types[level], ct_off, ops, pt_node, pt_off, 0, 0);
}

/**
 * `capop_captbl_cap_create_pgtbl` enables the creation of a
 * capability to a page-table node (`n`) at a specific level in a slot
 * in a last-level capability-table node.
 *
 * - `@ctcap` - The cap-table node in which we'll create the capability.
 * - `@ct_off` - The offset into that node of the capability to-be.
 * - `@level` - The level of the pgtbl node `n`.
 * - `@ops` - The allowed operations we wish to enable on the captbl.
 * - `@pt_node` - The page-table node in which we can find `n`.
 * - `@pt_off` - The offset into that node at which we find `n`.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_create_pgtbl(cos_cap_t ctcap, uword_t ct_off, uword_t level, cos_op_bitmap_t ops, cos_cap_t pt_node, cos_cap_t pt_off)
{
	const cos_cap_type_t types[COS_PGTBL_MAX_DEPTH] = {COS_CAP_TYPE_PGTBL_0, COS_CAP_TYPE_PGTBL_1, COS_CAP_TYPE_PGTBL_2, COS_CAP_TYPE_PGTBL_3};

	if (unlikely(level >= COS_PGTBL_MAX_DEPTH)) return -COS_ERR_WRONG_CAP_TYPE;

	return __capop_captbl_cap_create(ctcap, COS_OP_CAPTBL_CAP_CREATE_RESTBL, types[level], ct_off, ops, pt_node, pt_off, 0, 0);
}

/**
 * `capop_captbl_cap_remove` deletes a capability slot in the last
 * level of a capability-table. Once this completes successfully, the
 * quiescence window (grace period) of the capability slot starts.
 *
 * - `@ctcap` - Capability table node with slot to be deleted.
 * - `@ct_off` - Offset into that node of the slot.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_remove(cos_cap_t ctcap, uword_t ct_off)
{
	return __capop_4_1_op(ctcap, COS_OP_CAPTBL_CAP_REMOVE, ct_off, 0, 0, 0);
}

/**
 * `capop_captbl_cap_copy` copies a capability table slot
 * `ctcap_from[ct_from_off] -> ctcap_to[ct_to_off]`, giving it a
 * subset of the allowed operations of the original capability. Note
 * that the `ops` must be a subset of those of the existing (from)
 * capability and of the ops for each of the capability-table nodes.
 *
 * - `@ctcap_to` - Node with slot to be copied into.
 * - `@ct_to_off` - Slot within that node in which to create the cap.
 * - `@ctcap_from` - Node with capability to be copied from.
 * - `@ct_from_off` - Offset of the originating slot.
 * - `@ops` - Desired operations to give the new slot.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_captbl_cap_copy(cos_cap_t ctcap_to, uword_t ct_to_off, cos_cap_t ctcap_from, uword_t ct_from_off, cos_op_bitmap_t ops)
{
	return __capop_4_1_op(ctcap_to, COS_OP_RESTBL_CAP_COPY, ct_to_off, ctcap_from, ct_from_off, ops);
}

/***
 * Page-table system calls for retyping (thus memory management and
 * allocation) and shared memory (copying) into specific interesting
 * types.
 *
 * Constraints required to avoid an error reply:
 *
 * - The target page must be in the untyped state.
 * - The target page must be quiesced.
 *
 * Scalability:
 *
 * - We are attempting to retype the page, which requires updates to
 *   the *shared* global types of that page. Only multiple retype
 *   operations on multiple cores will conflict in this case, and
 *   these should be simple to avoid. This is one of the most
 *   unscalable operations of the system, currently. At least until we
 *   support per-core reference counts.
 */

/**
 * `capop_pgtbl_retype2pgtbl` attempts to retype an untyped page of
 * memory into a page-table node. Note that doing so will only be
 * successful after the page (that had previously been freed into
 * untyped memory) has quiesced.
 *
 * - `@ptcap` - The page-table node with the untyped memory to retype.
 * - `@pt_off` - ...and its offset into the page-table node
 * - `@level` - The level of the page-table.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_pgtbl_retype2pgtbl(cos_cap_t ptcap, uword_t pt_off, uword_t level)
{
	return __capop_4_1_op(ptcap, COS_OP_PGTBL_RETYPE_PGTBL, pt_off, level, 0, 0);
}

/**
 * `capop_pgtbl_retype2captbl` attempts to retype an untyped page of
 * memory into a capability table node. Note that doing so will only
 * be successful after the page (that had previously been freed into
 * untyped memory) has quiesced.
 *
 * - `@ptcap` - The page-table node with the untyped memory to retype.
 * - `@pt_off` - ...and its offset into the page-table node
 * - `@level` - The level of the capability table.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_pgtbl_retype2captbl(cos_cap_t ptcap, uword_t pt_off, uword_t level)
{
	return __capop_4_1_op(ptcap, COS_OP_PGTBL_RETYPE_PGTBL, pt_off, level, 0, 0);
}

/**
 * `capop_pgtbl_retype2thd` attempts to retype an untyped page of
 * memory into a thread. Note that doing so will only be
 * successful after the page (that had previously been freed into
 * untyped memory) has quiesced.
 *
 * - `@ptcap` - The page-table node with the untyped memory to retype.
 * - `@pt_off` - ...and its offset into the page-table node
 * - `@schedthd_cap` - The capability to the scheduler thread.
 * - `@comp_cap` - The component to create the thread in.
 * - `@tid` - The id to give the thread.
 * - `@tok` - The scheduler's token used to identify the thread in
 *            scheduler events.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_pgtbl_retype2thd(cos_cap_t ptcap, uword_t pt_off, cos_cap_t schedthd_cap, cos_cap_t comp_cap, thdid_t tid, id_token_t tok)
{
	return __capop_9_1_op(ptcap, COS_OP_PGTBL_RETYPE_THD, pt_off, schedthd_cap, comp_cap, tid, tok, 0, 0, 0, 0);
}

/**
 * `capop_pgtbl_retype2comp` attempts to retype an untyped page of
 * memory into a component. Note that doing so will only be
 * successful after the page (that had previously been freed into
 * untyped memory) has quiesced.
 *
 * - `@ptcap` - The page-table node with the untyped memory to retype.
 * - `@pt_off` - ...and its offset into the page-table node.
 * - `@captbl_cap` - The capability table to use for the component.
 * - `@pgtbl_cap` - The page-table to use for the component.
 * - `@tag` - The protection domain context (asid, mpk key) for the component.
 * - `@entry_addr` - The starting instruction pointer for the component.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_pgtbl_retype2comp(cos_cap_t ptcap, uword_t pt_off, cos_cap_t captbl_cap, cos_cap_t pgtbl_cap, prot_domain_tag_t tag, vaddr_t entry_addr)
{
	return __capop_9_1_op(ptcap, COS_OP_PGTBL_RETYPE_THD, pt_off, captbl_cap, pgtbl_cap, tag, entry_addr, 0, 0, 0, 0);
}

/**
 * `capop_pgtbl_retype2free` attempts to retype a typed page of
 * memory that is some kernel or user memory type into an untyped
 * page. This will fail if there are any references from other
 * resources to this page (i.e. if components reference a page-table
 * root note, it cannot be freed until those components are freed).
 * Note that references to a page from the capability-table are all
 * weak references, so capabilities don't need to be removed. Such
 * references will become unaccessible once the backing page is
 * retyped into untyped.
 *
 * When a page is moved into the untyped state, it starts the
 * quiescence process. If it is was kernel data-structure, quiescence
 * must await all cores no longer potentially accessing it (parallel
 * quiescence); if it was a page mapped into virtual memory,
 * quiescence must await TLB flushes that could contain the page. See
 * the documentation on quiescence for more details.
 *
 * We have the untyped state for pages -- as opposed to simply
 * retyping directly between different kernel and virtual mapping
 * states -- so that we can encode the "quiescing" state.
 *
 * Constraints required to avoid an error reply:
 *
 * - The target page must not be in the untyped state.
 * - The target page must not be quiescing.
 * - The target page must not be referenced by other pages.
 *
 * Scalability:
 *
 * - We are attempting to retype the page, which requires updates to
 *   the *shared* global types of that page. Only multiple retype
 *   operations on multiple cores will conflict in this case, and
 *   these should be simple to avoid. This is one of the most
 *   unscalable operations of the system, currently.
 * - This operation inherently requires checking if the page's
 *   reference count has dropped to zero. This might be *less*
 *   scalable once we support per-core reference counts.
 *
 * Arguments:
 *
 * - `@ptcap` - The page-table node with the untyped memory to retype.
 * - `@pt_off` - ...and its offset into the page-table node
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_pgtbl_retype2free(cos_cap_t ptcap, uword_t pt_off)
{
	return __capop_4_1_op(ptcap, COS_OP_PGTBL_RETYPE_DEALLOCATE, pt_off, 0, 0, 0);
}

/**
 * `capop_pgtbl_ref_copy` copies a page-table reference to a page.
 * In the case where the reference is to a virtual memory mapping,
 * this will create shared memory. This operation is *not necessarily
 * scalable* in the sense that it will increment a reference count for
 * the resource (until we support per-core reference counts).
 *
 * The `perm` reference permission are a little complex. They
 * generally must be a *subset* of the previous mappings values --
 * which must not include "present" for kernel data-structures. The
 * exception is this: the "present" bit can be added to a copy of
 * pages of virtual memory types, thus making it visible.
 *
 * - `@ptcap_to` - page-table node to copy reference into.
 * - `@pt_to_off` - offset in that node for copied reference.
 * - `@ptcap_from` - page-table node to copy reference from.
 * - `@pt_from_off` - offset in that node for original reference.
 * - `@perm` - permissions for the new mapping.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_pgtbl_ref_copy(cos_cap_t ptcap_to, uword_t pt_to_off, cos_cap_t ptcap_from, uword_t pt_from_off, uword_t perm)
{
	return __capop_4_1_op(ptcap_to, COS_OP_RESTBL_CAP_COPY, pt_to_off, ptcap_from, pt_from_off, perm);
}

/***
 * Capability and page table system calls to construct and deconstruct
 * the tables. Hooks (or de-hooks) a node at level N+1
 * (`ctcap_bottom`) into a specific entry (at offset `ct_off`) at
 * level N (`ctcap_top`).
 */

/**
 * `capop_restbl_construct` is used to construct resource table
 * tries. Specifically, it is used to hook up a node at level N+1 into
 * a specific entry in a node at level N. This is the function that
 * enables the "hierarchical" in hierarchical page-tables and
 * capability tables.
 *
 * Constraints required to avoid an error reply:
 *
 * - The nodes must be at levels N and N+1.
 * - The resource table nodes must be of the correct types.
 * - The slot in the top resource table must be empty.
 *
 * Scalability:
 *
 * - This will update the resource table slot, which is accessible on
 *   all cores. The necessary CAS for this might conflict with
 *   parallel operations to the same slot, but there is no
 *   retry/looping, so this is unlikely to cause much overhead.
 *   However, these slots are the size of a word, so CASes to words on
 *   the same cache-line might cause more cache-line bouncing.
 *   Experiments must be done on repetitive updates to two slots on
 *   the same cache-line to assess the impact on response times of
 *   this syscall.
 * - This updates reference counts for both the `rtcap_top` and
 *   `rtcap_bottom` nodes, thus might not scale. Note that this might
 *   only threaten scalability in the case where we have frequent
 *   aliasing *within* the structure of the resource table, which is
 *   not very common. Scalability issues might be solved with per-core
 *   counters in the future.
 *
 * Arguments:
 *
 * - `@rtcap_top` - node at level N.
 * - `@rt_off` - offset into that node to identify entry we'll update.
 * - `@rtcap_bottom` - node at level N+1 added at that entry.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_restbl_construct(cos_cap_t rtcap_top, uword_t rt_off, uword_t rtcap_bottom)
{
	return __capop_4_1_op(rtcap_top, COS_OP_RESTBL_CONSTRUCT, rt_off, rtcap_bottom, 0, 0);
}

/**
 * `capop_restbl_construct` is used to construct resource table
 * tries. Specifically, it is used to hook up a node at level N+1 into
 * a specific entry in a node at level N. This is the function that
 * enables the "hierarchical" in hierarchical page-tables and
 * capability tables.
 *
 * Constraints required to avoid an error reply:
 *
 * - The resource table node must be of the correct type.
 * - The slot in the resource table must be non-empty.
 *
 * Scalability:
 *
 * - This will update the resource table slot, which is accessible on
 *   all cores. See the comment above for `capop_restbl_construct`.
 * - This updates reference counts for the `rtcap_bottom` node, thus
 *   might not scale. Note that this might only threaten scalability
 *   in the case where we have frequent aliasing *within* the
 *   structure of the resource table, which is not very common.
 *   Scalability issues might be solved with per-core counters in the
 *   future.
 *
 * Arguments:
 *
 * - `@rtcap_top` - node at level N.
 * - `@rt_off` - offset into that node to identify entry we'll update.
 * - `@rtcap_bottom` - node at level N+1 added at that entry.
 * - `@return` - `COS_RET_SUCCESS` or a negative `COS_ERR_*` on error.
 */
COS_FORCE_INLINE static inline cos_retval_t
capop_restbl_deconstruct(cos_cap_t rtcap_top, uword_t rt_off)
{
	return __capop_4_1_op(rtcap_top, COS_OP_RESTBL_CONSTRUCT, rt_off, 0, 0, 0);
}

/***
 * Hardware capability system calls. For now, we'll focus only on
 * printing.
 *
 * TODO: use 9 registers to pass longer strings.
 */

COS_FORCE_INLINE static inline cos_retval_t
capop_hw_print(cos_cap_t hwcap, uword_t len, uword_t a0, uword_t a1, uword_t a2, uword_t *written)
{
	uword_t ret;
	const uword_t max = sizeof(uword_t) * 3;

	*written = 0;
	if (len > max) len = max;
	cos_syscall_4_4(hwcap, COS_OP_HW_PRINT, len, a0, a1, a2, &ret, written, 0, 0);

	return ret;
}
