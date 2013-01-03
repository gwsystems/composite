/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright 2007 by Boston University.
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 *
 * Copyright The George Washington University, Gabriel Parmer,
 * gparmer@gwu.edu, 2009
 */

#include "include/ipc.h"
#include "include/spd.h"
#include "include/debug.h"
#include "include/measurement.h"
#include "include/mmap.h"
#include "include/per_cpu.h"
#include "include/chal.h"
#include <linux/kernel.h>

/* 
 * These are the 1) page for the pte for the shared region and 2) the
 * page to hold general data including cpuid, thread id, identity
 * info, etc...  These are placed here so that we don't have to go
 * through a variable to find their address, so that lookup and
 * manipulation are quick.
 */
unsigned int shared_region_page[1024] PAGE_ALIGNED;
unsigned int shared_data_page[1024] PAGE_ALIGNED;

static inline struct shared_user_data *get_shared_data(void)
{
	return (struct shared_user_data*)shared_data_page;
}

#define COS_SYSCALL __attribute__((regparm(0)))

/* 
 * This variable tracks the number of cycles that have elapsed since
 * the last measurement and is typically used to measure how long
 * brand threads execute.
 */
static unsigned long cycle_cnt;

void 
ipc_init(void)
{
	memset(shared_data_page, 0, PAGE_SIZE);
	rdtscl(cycle_cnt);

	return;
}

static inline void 
open_spd(struct spd_poly *spd)
{
	printk("cos: open_spd (asymmetric trust) not supported on x86.\n");
	
	return;
}

static inline void 
switch_pgtbls(paddr_t new, paddr_t old)
{
	if (likely(old != new)) {
		chal_pgtbl_switch(new);
	}

	return;
}

static inline void 
open_close_spd(struct spd_poly *o_spd, struct spd_poly *c_spd)
{
	switch_pgtbls(o_spd->pg_tbl, c_spd->pg_tbl);

	return;
}

static inline void 
open_close_spd_ret(struct spd_poly *c_spd)
{
	chal_pgtbl_switch(c_spd->pg_tbl);
	
	return;
}

extern struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];

struct inv_ret_struct {
	int thd_id;
	int spd_id;
};


/* 
 * FIXME: 1) should probably return the static capability to allow
 * isolation level isolation access from caller, 2) all return 0
 * should kill thread.
 */

COS_SYSCALL vaddr_t 
ipc_walk_static_cap(unsigned int capability, vaddr_t sp, 
		    vaddr_t ip, struct inv_ret_struct *ret)
{
	struct thd_invocation_frame *curr_frame;
	struct spd *curr_spd, *dest_spd;
	struct invocation_cap *cap_entry;
	struct thread *thd = core_get_curr_thd_id(get_cpuid_fast());

	capability >>= 20;

	if (unlikely(capability >= MAX_STATIC_CAP)) {
		struct spd *t = virtual_namespace_query(ip);
		printk("cos: capability %d greater than max from spd %d @ %x.\n", 
		       capability, (t) ? spd_get_index(t): 0, (unsigned int)ip);
		return 0;
	}

	cap_entry = &invocation_capabilities[capability];

	if (unlikely(!cap_entry->owner)) {
		printk("cos: No owner for cap %d.\n", capability);
		return 0;
	}

	/* what spd are we in (what stack frame)? */
	curr_frame = &thd->stack_base[thd->stack_ptr];
	dest_spd = cap_entry->destination;
	curr_spd = cap_entry->owner;

	if (unlikely(!dest_spd || curr_spd == CAP_FREE || curr_spd == CAP_ALLOCATED_UNUSED)) {
		printk("cos: Attempted use of unallocated capability.\n");
		return 0;
	}
	/*
	 * If the spd that owns this capability is part of a composite
	 * spd that is the same as the composite spd that was the
	 * entry point for this composite spd.
	 *
	 * i.e. is the capability owner in the same protection domain
	 * (via ST) as the spd entry point to the protection domain.
	 *
	 * We are doing a repetitive calculation for the first check
	 * here and in the thd_spd_in_current_composite, as we want to
	 * avoid making the function call here if possible.  FIXME:
	 * should just use a specific inlined method here to avoid
	 * this.
	 */
	if (unlikely(!thd_spd_in_composite(curr_frame->current_composite_spd, curr_spd))) {
		printk("cos: Error, incorrect capability (Cap %d has spd %d, stk @ %d has %d).\n",
		       capability, spd_get_index(curr_spd), thd->stack_ptr, spd_get_index(curr_frame->spd));
		/* 
		 * FIXME: do something here like throw a fault to be
		 * handled by a user-level handler
		 */
		return 0;
	}
	/* now we are committing to the invocation */
	cos_meas_event(COS_MEAS_INVOCATIONS);

	open_close_spd(dest_spd->composite_spd, curr_spd->composite_spd);

	/* Updating current spd: not used for now. */
	/* core_put_curr_spd(&(dest_spd->spd_info)); */

	ret->thd_id = thd->thread_id | (get_cpuid_fast() << 16);
	ret->spd_id = spd_get_index(curr_spd);

	spd_mpd_ipc_take((struct composite_spd *)dest_spd->composite_spd);

	/* add a new stack frame for the spd we are invoking (we're committed) */
	thd_invocation_push(thd, cap_entry->destination, sp, ip);
	cap_entry->invocation_cnt++;

	return cap_entry->dest_entry_instruction;
}

static struct pt_regs *brand_execution_completion(struct thread *curr, int *preempt);
static struct pt_regs *thd_ret_term_upcall(struct thread *t);
static struct pt_regs *thd_ret_upcall_type(struct thread *t, upcall_type_t type);
/*
 * Return from an invocation by popping off of the invocation stack an
 * entry, and returning its contents (including return ip and sp).
 * This is complicated by the fact that we may return when no
 * invocation is made because a thread is terminating.
 */
COS_SYSCALL struct thd_invocation_frame *
pop(struct pt_regs **regs_restore)
{
	struct thd_invocation_frame *inv_frame;
	struct thd_invocation_frame *curr_frame;

	struct thread *curr = core_get_curr_thd_id(get_cpuid_fast());

	inv_frame = thd_invocation_pop(curr);

	/* At the top of the invocation stack? */
	if (unlikely(inv_frame == NULL)) {
		assert(!(curr->flags & THD_STATE_READY_UPCALL));

		/* normal thread terminates: upcall into root
		 * scheduler */
		*regs_restore = thd_ret_term_upcall(curr);

		return NULL;
	}
	
	curr_frame = thd_invstk_top(curr);
	/* for now just assume we always close the server spd */
	open_close_spd_ret(curr_frame->current_composite_spd);

	/*
	 * FIXME: If an invocation causes a "needless" kernel
	 * invocation/pop even when the two spds are in the same
	 * composite spd (but weren't at some point in the future),
	 * then we really probably shouldn't release here.
	 *
	 * This REALLY should be spd_mpd_release.
	 */
	spd_mpd_ipc_release((struct composite_spd *)inv_frame->current_composite_spd);

	/* Fault caused initial invocation.  FIXME: can we get this off the common case path? */
	if (unlikely(inv_frame->ip == 0)) {
		*regs_restore = &curr->fault_regs;
		return NULL;
	}

	return inv_frame;	
}

/* return 1 if the fault is handled by a component */
int 
fault_ipc_invoke(struct thread *thd, vaddr_t fault_addr, int flags, struct pt_regs *regs, int fault_num)
{
	struct spd *s = virtual_namespace_query(regs->ip);
	struct thd_invocation_frame *curr_frame;
	struct inv_ret_struct r;
	vaddr_t a;
	unsigned int fault_cap;
	struct pt_regs *nregs;

	/* printk("thd %d, fault addr %p, flags %d, fault num %d\n", thd_get_id(thd), fault_addr, flags, fault_num); */
	/* corrupted ip? */
	if (unlikely(!s)) {
		curr_frame = thd_invstk_top(thd);
		s = curr_frame->spd;
	}
	assert(fault_num < COS_FLT_MAX);

	fault_cap = s->fault_handler[fault_num];
	/* If no component catches this fault, upcall into the
	 * scheduler with a "destroy thread" event. */
	if (unlikely(!fault_cap)) {
		nregs = thd_ret_upcall_type(thd, COS_UPCALL_UNHANDLED_FAULT);
#define COPY_REG(name) regs-> name = nregs-> name
		COPY_REG(ax);
		COPY_REG(bx);
		COPY_REG(cx);
		COPY_REG(dx);
		COPY_REG(di);
		COPY_REG(si);
		COPY_REG(bp);
		COPY_REG(sp);
		COPY_REG(ip);
		return 0;
	}
	
	/* save the faulting registers */
	memcpy(&thd->fault_regs, regs, sizeof(struct pt_regs));
	a = ipc_walk_static_cap(fault_cap<<20, regs->sp, regs->ip, &r);

	/* setup the registers for the fault handler invocation */
	regs->ax = r.thd_id;
	regs->bx = regs->cx = r.spd_id;
	regs->sp = 0;
	/* arguments (including bx above) */
	regs->si = fault_addr;
	regs->di = flags;
	regs->bp = regs->ip;

	/* page fault handler address */
	regs->dx = regs->ip = a;

	return 1;
}

/********** Composite system calls **********/

COS_SYSCALL int 
cos_syscall_void(int spdid)
{
	printk("cos: error - %d made void system call from %d\n", thd_get_id(core_get_curr_thd()), spdid);

	return 0;
}

extern int switch_thread_data_page(int old_thd, int new_thd);

static inline void __switch_thread_context(struct thread *curr, struct thread *next, 
					   struct spd_poly *cspd, struct spd_poly *nspd)
{
	struct shared_user_data *ud = get_shared_data();
	unsigned int ctid, ntid;

	assert(core_get_curr_thd() != next);

	ctid = thd_get_id(curr);
	ntid = thd_get_id(next);

	core_put_curr_thd(next);
	core_put_curr_spd(nspd);

	/* thread ids start @ 1, thus thd pages are offset above the data page */
	ud->current_thread = ntid;
	ud->argument_region = (void*)((ntid * PAGE_SIZE) + COS_INFO_REGION_ADDR);

	return;

}

static inline void switch_thread_context(struct thread *curr, struct thread *next)
{
	struct spd_poly *nspd, *cspd;

	cspd = thd_get_thd_spdpoly(curr);
	nspd = thd_get_thd_spdpoly(next);
	__switch_thread_context(curr, next, cspd, nspd);
	open_close_spd(nspd, cspd);
}

void initialize_sched_info(struct thread *t, struct spd *curr_sched)
{
	struct spd *sched;
	int i;

	assert(spd_is_scheduler(curr_sched));
	/* 
	 * Initialize the thread's path through its hierarchy of
	 * schedulers.  They will have to explicitly set the
	 * thread_notification location at a later time.
	 *
	 * OPTION: Another option here would be to simply copy the
	 * scheduler hierarchy of the current thread.  A good way to
	 * initialize the urgency values for all the schedulers.
	 */
	sched = curr_sched;
	for (i = curr_sched->sched_depth ; i >= 0 ; i--) {
		struct thd_sched_info *tsi = thd_get_sched_info(t, i);

		tsi->scheduler = sched;
		tsi->thread_notifications = NULL;

		sched = sched->parent_sched;
	}

	return;
}

/*
 * Note here that we still copy the empty structures for simplicity.
 */
static inline void copy_sched_info_structs(struct thd_sched_info *new, 
					   struct thd_sched_info *old, int num)
{
	int i;
	struct spd *prev_sched = NULL;

	assert(num < MAX_SCHED_HIER_DEPTH);

	for (i = 0 ; i < num ; i++) {
		if (old[i].scheduler) {
			assert(old[i].scheduler->parent_sched == prev_sched);
		}

		prev_sched = old[i].scheduler;
		new[i].scheduler = prev_sched;
		new[i].thread_notifications = old[i].thread_notifications;
		new[i].notification_offset = old[i].notification_offset;
	}

	return;
}

void copy_sched_info(struct thread *new, struct thread *old)
{
	copy_sched_info_structs(new->sched_info, old->sched_info, MAX_SCHED_HIER_DEPTH-1);
}

/* 
 * Hope the current thread saved its context...should be able to
 * resume_return to it.
 *
 * TODO: should NOT pass in fn and stack here.  Should simply upcall
 * into cos_upcall_entry, and pass in the data.  The data should point
 * to the fn and stack to be used anyway, which can be assigned at
 * user-level.
 */
COS_SYSCALL int 
cos_syscall_create_thread(int spd_id, int a, int b, int c)
{
	struct thread *thd, *curr;
	struct spd *curr_spd;

	/*
	 * Lets make sure that the current spd is a scheduler and has
	 * scheduler control of the current thread before we let it
	 * create any threads.
	 *
	 * FIXME: in the future, I should really just allow the base
	 * scheduler to create threads, i.e. when 0 == sched_depth.
	 */
	curr = core_get_curr_thd();
	curr_spd = thd_validate_get_current_spd(curr, spd_id);
	if (NULL == curr_spd) {
		printk("cos: component claimed in spd %d, but not\n", spd_id);
		return -1;
	}

	if (!spd_is_scheduler(curr_spd)/* || !thd_scheduled_by(curr, curr_spd)*/) {
		/* 
		 * FIXME: if initmm is the root, then the second to
		 * root should be able to create threads. 
		 */
		//	if (!spd_is_root_sched(curr_spd)) {
		printk("cos: non-scheduler attempted to create thread.\n");
		return -1;
	}

	thd = thd_alloc(curr_spd);
	if (thd == NULL) {
		printk("cos: Could not allocate thread\n");
		return -1;
	}

	/* FIXME: switch to using upcall_setup here */
	thd->stack_ptr = 0;
	thd->stack_base[0].current_composite_spd = curr_spd->composite_spd;
	thd->stack_base[0].spd = curr_spd;
	/* FIXME: do this lazily */
	spd_mpd_ipc_take((struct composite_spd *)curr_spd->composite_spd);

	thd->regs.cx = COS_UPCALL_CREATE;
	thd->regs.dx = curr_spd->upcall_entry;
	thd->regs.bx = a;
	thd->regs.di = b;	
	thd->regs.si = c;
	thd->regs.ax = thd_get_id(thd) | (get_cpuid() << 16);

	thd->flags |= THD_STATE_CYC_CNT;
	initialize_sched_info(thd, curr_spd);
	
	return thd_get_id(thd);
}

COS_SYSCALL int 
cos_syscall_thd_cntl(int spd_id, int op_thdid, long arg1, long arg2)
{
	struct thread *thd, *curr;
	struct spd *curr_spd;
	short int op, thdid;

	op = op_thdid >> 16;
	thdid = op_thdid & 0xFFFF;
	/*
	 * Lets make sure that the current spd is a scheduler and has
	 * scheduler control of the current thread before we let it
	 * create any threads.
	 *
	 * FIXME: in the future, I should really just allow the base
	 * scheduler to create threads, i.e. when 0 == sched_depth.
	 */
	curr = core_get_curr_thd();
	curr_spd = thd_validate_get_current_spd(curr, spd_id);
	if (NULL == curr_spd) {
		printk("cos: component claimed in spd %d, but not\n", spd_id);
		return -1;
	}
	
	thd = thd_get_by_id(thdid);
	/* FIXME: finer grained access control required */
/*	if (!spd_is_scheduler(curr_spd) || !thd_scheduled_by(thd, curr_spd)) {
		printk("cos: non-scheduler attempted to manipulate thread.\n");
		return -1;
	}
*/
	
	switch (op) {
	case COS_THD_INV_FRAME:
	{
		struct spd *i_spd;
		int frame_offset = arg1;
		struct thd_invocation_frame *tif;

		tif = thd_invstk_nth(thd, frame_offset);
		if (NULL == tif) return 0;
		i_spd = tif->spd;
		return spd_get_index(i_spd);
	}
	case COS_THD_INV_FRAME_REM:
	{
		int frame_offset = arg1;

		if (thd == curr && frame_offset < 1)       return -1;
		if (thd_invstk_rem_nth(thd, frame_offset)) return -1;

		return 0;
	}
	case COS_THD_INV_SPD:
	{
		struct thd_invocation_frame *tif;
		int i;

		for (i = 0 ; (tif = thd_invstk_nth(thd, i)) ; i++) {
			if (arg1 == spd_get_index(tif->spd)) return i;
		}
		return -1;
	}
	case COS_THD_INVFRM_IP:
	{
		int frame_offset = arg1;

		return thd_get_frame_ip(thd, frame_offset);
	}
	case COS_THD_INVFRM_SET_IP:
	{
		int frame_offset = arg1;

		return thd_set_frame_ip(thd, frame_offset, arg2);
	}
	case COS_THD_INVFRM_SP:
	{
		int frame_offset = arg1;

		return thd_get_frame_sp(thd, frame_offset);
	}
	case COS_THD_INVFRM_SET_SP:
	{
		int frame_offset = arg1;

		return thd_set_frame_sp(thd, frame_offset, arg2);
	}
#define __GET_REG(name)							\
	{								\
		if (arg1) return thd->fault_regs. name ;		\
		if (!(thd->flags & THD_STATE_PREEMPTED)) return 0;	\
		return thd->regs. name ;				\
	}
	case COS_THD_GET_IP: __GET_REG(ip);
	case COS_THD_GET_SP: __GET_REG(sp);
	case COS_THD_GET_FP: __GET_REG(bp);
	case COS_THD_GET_1:  __GET_REG(ax);
	case COS_THD_GET_2:  __GET_REG(bx);
	case COS_THD_GET_3:  __GET_REG(cx);
	case COS_THD_GET_4:  __GET_REG(dx);
	case COS_THD_GET_5:  __GET_REG(di);
	case COS_THD_GET_6:  __GET_REG(si);
#define __SET_REG(name)							\
	{							        \
		if (arg2) thd->fault_regs. name = arg1;			\
		else if ((thd->flags & THD_STATE_PREEMPTED)) thd->regs. name = arg1; \
		else return -1;						\
		return 0;						\
	}
	case COS_THD_SET_IP: __SET_REG(ip);
	case COS_THD_SET_SP: __SET_REG(sp);
	case COS_THD_SET_FP: __SET_REG(bp);
	case COS_THD_SET_1:  __SET_REG(ax);
	case COS_THD_SET_2:  __SET_REG(bx);
	case COS_THD_SET_3:  __SET_REG(cx);
	case COS_THD_SET_4:  __SET_REG(dx);
	case COS_THD_SET_5:  __SET_REG(di);
	case COS_THD_SET_6:  __SET_REG(si);
	case COS_THD_STATUS:
	{
		/* FIXME: all flags should NOT be part of the ABI.
		 * Filter out relevant ones (upcall, brand,
		 * preempted) */
		return thd->flags;
	}
	default:
		printk("cos: undefined operation %d for thread %d from scheduler %d.\n",
		       op, thdid, spd_id);
		return -1;
	}
	return 0;
}

/* 
 * An upcall thread has upcalled into the scheduler so as it to
 * schedule due to the completion of the upcall.  This has pushed the
 * scheduler's entry onto its invocation stack.  But when this upcall
 * then calls switch_thread with the TAILCALL option, it must return
 * to its previous (brand's) protection domain.  Thus, pop off that
 * entry, and save its register information.
 *
 * The second argument is used to return the composite spd that was
 * upcalled into to execute the scheduler.
 */
static inline int 
sched_tailcall_adjust_invstk(struct thread *t)
{
	struct thd_invocation_frame *tif, *ntif;
	struct spd *s;
	struct spd_poly *nspd, *cspd;
	
	assert(t && t->flags & THD_STATE_ACTIVE_UPCALL && t->thread_brand);
	
	tif = thd_invocation_pop(t);
	assert(tif);
	/* saving to both ip and edx allows this to correctly return
	 * to the thread's execution regardless of if it is invoked
	 * via loading registers, or via sysexit. */
	t->regs.ip = t->regs.dx = tif->ip;
	t->regs.sp = t->regs.cx = tif->sp;
	s = tif->spd;
	cspd = tif->current_composite_spd;
	ntif = thd_invstk_top(t);
	if (NULL == ntif) return -1;
	nspd = ntif->current_composite_spd;

	open_close_spd(nspd, cspd);

	spd_mpd_ipc_release((struct composite_spd *)tif->current_composite_spd);

	return spd_get_index(s);
}

static inline void 
remove_preempted_status(struct thread *thd)
{
 	if (thd->preempter_thread) {
		struct thread *p = thd->preempter_thread;
		struct thread *i = thd->interrupted_thread;

		/* is the doubly linked list sound? */
		assert(p->interrupted_thread == thd);
			
		/* break the doubly linked list of interrupted thds */
		p->interrupted_thread = NULL;
		if (i) i->preempter_thread = NULL;
		thd->preempter_thread = NULL;
		thd->interrupted_thread = NULL;
	}

	thd->flags &= ~THD_STATE_PREEMPTED;
}

extern int cos_syscall_switch_thread(void);
static void update_sched_evts(struct thread *new, int new_flags, 
		       struct thread *prev, int prev_flags);
static struct pt_regs *sched_tailcall_pending_upcall(struct thread *uc, 
						     struct composite_spd *curr);
static struct thread *sched_tailcall_pending_upcall_thd(struct thread *uc, 
							 struct composite_spd *curr);
static inline void update_thd_evt_state(struct thread *t, int flags, unsigned long elapsed_cycles);
static inline void 
break_preemption_chain(struct thread *t)
{
	struct thread *other;

	cos_meas_event(COS_MEAS_BREAK_PREEMPTION_CHAIN);	
	other = t->interrupted_thread;
	if (unlikely(other)) {
		assert(other->preempter_thread == t);
		t->interrupted_thread = NULL;
		other->preempter_thread = NULL;
	}
	other = t->preempter_thread;
	if (unlikely(other)) {
		assert(other->interrupted_thread == t);
		t->preempter_thread = NULL;
		other->interrupted_thread = NULL;
	}
}

static inline unsigned short int 
switch_thread_parse_data_area(struct cos_sched_data_area *da, int *ret_code)
{
	unsigned short int next_thd;

	if (unlikely(da->cos_evt_notif.pending_event)) {
		cos_meas_event(COS_MEAS_RESCHEDULE_PEND);
		*ret_code = COS_SCHED_RET_AGAIN;
		goto ret_err;
	}
	if (unlikely(da->cos_evt_notif.pending_cevt)) {
		cos_meas_event(COS_MEAS_RESCHEDULE_CEVT);
		*ret_code = COS_SCHED_RET_CEVT;
		goto ret_err;
	}

	next_thd = da->cos_next.next_thd_id;
	da->cos_next.next_thd_id = 0;
	if (unlikely(0 == next_thd)) {
		*ret_code = COS_SCHED_RET_AGAIN;
		goto ret_err;
	}
	/* FIXME: mask out the locking flags as they cannot apply */
	return next_thd;
ret_err:
	return 0;
}

static inline struct thread *
switch_thread_get_target(unsigned short int tid, struct thread *curr, 
			 struct spd *curr_spd, int *ret_code)
{
	struct thread *thd;

	thd = thd_get_by_id(tid);
	/* error cases */
	if (unlikely(thd == curr)) {
		cos_meas_event(COS_MEAS_SWITCH_SELF);
		*ret_code = COS_SCHED_RET_AGAIN;
		goto ret_err;
	}
	if (unlikely(NULL == thd)) {
		/* 
		 * Uncommon, but valid case: between when the current thread
		 * executed through the scheduler and when the switch_thread
		 * system call was made, that thread was preempted, or an
		 * event occurred (which zeros out tid).  This simply
		 * means that the current system call doesn't have all the
		 * information (which upcalls are active, if threads have been
		 * woken up, etc...), so we should make it go through the
		 * scheduling process again.
		 */
		if (unlikely(0 == tid)) {
			cos_meas_event(COS_MEAS_SWITCH_OUTDATED);
			*ret_code = COS_SCHED_RET_AGAIN;
		} else {
			*ret_code = COS_SCHED_RET_INVAL;
		}
		/* error otherwise */
		goto ret_err;
	}


	/* We have valid threads, lets make sure we can schedule them! */
	if (unlikely(!thd_scheduled_by(curr, curr_spd) ||
		     !thd_scheduled_by(thd, curr_spd))) {
		*ret_code = COS_SCHED_RET_ERROR;
		/* printk("curr %d sched by %d, thd %d sched by %d.\n", thd_get_id(curr), spd_get_index(thd_get_sched_info(curr, curr_spd->sched_depth)->scheduler),  */
		/*        thd_get_id(thd), spd_get_index(thd_get_sched_info(thd, curr_spd->sched_depth)->scheduler)); */
		goto ret_err;
	}

	/* we cannot schedule to run an upcall thread that is not running */
	if (unlikely(thd->flags & THD_STATE_READY_UPCALL)) {
		/* printk("args: tid %u, curr thd %d, curr spd %p, \n thd id %d is upcall thd...", tid, thd_get_id(curr), curr_spd, thd_get_id(thd)); */
		cos_meas_event(COS_MEAS_UPCALL_INACTIVE);
		*ret_code = COS_SCHED_RET_INVAL;
		goto ret_err;
	}
	
	return thd;
ret_err:
	return NULL;
}

static struct thread *
switch_thread_slowpath(struct thread *curr, unsigned short int flags, struct spd *curr_spd, 
		       unsigned short int rthd_id, struct cos_sched_data_area *da,
		       int *ret_code, unsigned short int *curr_flags, 
		       unsigned short int *thd_flags);

static inline void 
switch_thread_update_flags(struct cos_sched_data_area *da, unsigned short int *flags)
{
	if (likely(!(da->cos_next.next_thd_flags & COS_SCHED_CHILD_EVT))) 
		*flags &= ~COS_SCHED_CHILD_EVT;
	else 
		*flags |= COS_SCHED_CHILD_EVT;
}

 // print out on switch_thread errors???
#ifdef NIL
#define goto_err(label, format, args...)			\
	do {							\
		printk(format, ## args);			\
		goto label ;					\
	} while (0)
#else
#define goto_err(label, format, args...) goto label
#endif

/*
 * The arguments are horrible as we are interfacing w/ assembly and 1)
 * we need to return two values, the regs to restore, and if the next
 * thread was preempted or not (totally different return sequences),
 * 2) all syscalls provide as the first argument the spd_id of the spd
 * making the syscalls.  We need this info, and it is on the stack,
 * but (1) interferes with that as it pushes values onto the stack.  A
 * more pleasant way to deal with this might be to pass the args in
 * registers.  see ipc.S cos_syscall_switch_thread.
 */

COS_SYSCALL struct pt_regs *
cos_syscall_switch_thread_cont(int spd_id, unsigned short int rthd_id, 
			       unsigned short int rflags, long *preempt)
{
	struct thread *thd, *curr;
	struct spd *curr_spd;
	unsigned short int next_thd, flags;
	unsigned short int curr_sched_flags = COS_SCHED_EVT_NIL,
		           thd_sched_flags  = COS_SCHED_EVT_NIL;
	struct cos_sched_data_area *da;
	int ret_code = COS_SCHED_RET_ERROR;

	*preempt = 0;
	curr = core_get_curr_thd();
	/* printk("thd %d, switch thd core %d\n", thd_get_id(curr), get_cpuid()); */

	curr_spd = thd_validate_get_current_spd(curr, spd_id);
	if (unlikely(!curr_spd)) {
		printk("err: wrong spd!\n");
		goto ret_err;
	}

	assert(!(curr->flags & THD_STATE_PREEMPTED));

	/* Probably should change to kern_sched_shared_page */
	da = curr_spd->sched_shared_page[get_cpuid()];
	if (unlikely(!da)) {
		printk("err: no shared data area!\n");
		goto ret_err;
	}

	/* 
	 * So far all flags should be taken in the context of the
	 * actual invoking thread (they effect the thread switching
	 * _from_ rather than the thread to switch _to_) in which case
	 * we would want to use the sched_page flags.
	 */
	flags = rflags;
	switch_thread_update_flags(da, &flags);

	if (unlikely(flags)) {
		thd = switch_thread_slowpath(curr, flags, curr_spd, rthd_id, da, &ret_code, 
					     &curr_sched_flags, &thd_sched_flags);
		/* If we should return immediately back to this
		 * thread, and its registers have been changed,
		 * return without setting the return value */
		if (ret_code == COS_SCHED_RET_SUCCESS && thd == curr) goto ret;
		if (thd == curr) 
		{
			printk("err: thd == curr, ret %d\n", ret_code);
			goto_err(ret_err, "sloooow\n");
		}
	} else {
		next_thd = switch_thread_parse_data_area(da, &ret_code);
		if (unlikely(0 == next_thd)) {
			printk("err: data area\n");
			goto_err(ret_err, "data_area\n");
		}

		thd = switch_thread_get_target(next_thd, curr, curr_spd, &ret_code);

		if (unlikely(NULL == thd)) {
			printk("err: get target\n");
			goto_err(ret_err, "get target");
		}
	}

	/* If a thread is involved in a scheduling decision, we should
	 * assume that any preemption chains that existed aren't valid
	 * anymore. */
	break_preemption_chain(curr);

	switch_thread_context(curr, thd);
	if (thd->flags & THD_STATE_PREEMPTED) {
		cos_meas_event(COS_MEAS_SWITCH_PREEMPT);
		remove_preempted_status(thd);
		*preempt = 1;
	} else {
		cos_meas_event(COS_MEAS_SWITCH_COOP);
	}

	update_sched_evts(thd, thd_sched_flags, curr, curr_sched_flags);
	/* success for this current thread */
	curr->regs.ax = COS_SCHED_RET_SUCCESS;
//	printk("core %d: switch %d -> %d\n", get_cpuid(), thd_get_id(curr), thd_get_id(thd));
	event_record("switch_thread", thd_get_id(curr), thd_get_id(thd));

	return &thd->regs;
ret_err:
	curr->regs.ax = ret_code;
ret:
	return &curr->regs;
}

static struct thread *
switch_thread_slowpath(struct thread *curr, unsigned short int flags, struct spd *curr_spd, 
		       unsigned short int rthd_id, struct cos_sched_data_area *da,
		       int *ret_code, unsigned short int *curr_flags, 
		       unsigned short int *thd_flags)
{
	struct thread *thd;
	unsigned short int next_thd;

	if (flags & (COS_SCHED_SYNC_BLOCK | COS_SCHED_SYNC_UNBLOCK)) {
		next_thd = rthd_id;
		/* FIXME: mask out all flags that can't apply here  */
	} else {
		next_thd = switch_thread_parse_data_area(da, ret_code);
		if (unlikely(!next_thd)) goto_err(ret_err, "data area\n");
	}

	thd = switch_thread_get_target(next_thd, curr, curr_spd, ret_code);
	if (unlikely(NULL == thd)) goto_err(ret_err, "get_target");

	if (flags & (COS_SCHED_TAILCALL | COS_SCHED_BRAND_WAIT)) {
		/* First make sure this is an active upcall */
		if (unlikely(!(curr->flags & THD_STATE_ACTIVE_UPCALL))) goto ret_err;

		assert(!(curr->flags & THD_STATE_READY_UPCALL));
		/* Can't really be tailcalling and have the other
		 * flags at the same time */
		if (unlikely(flags & (COS_SCHED_SYNC_BLOCK | COS_SCHED_SYNC_UNBLOCK))) 
			goto_err(ret_err, "tailcall and block???\n");

		cos_meas_stats_end(COS_MEAS_STATS_UC_TERM_DELAY, 1);
		cos_meas_stats_end(COS_MEAS_STATS_UC_PEND_DELAY, 0);

		if (flags & COS_SCHED_TAILCALL && 
		    unlikely(spd_get_index(curr_spd) != sched_tailcall_adjust_invstk(curr))) 
			goto_err(ret_err, "tailcall from incorrect spd!\n");
		
		assert(curr->thread_brand);
		if (curr->thread_brand->pending_upcall_requests) {
			//update_thd_evt_state(curr, COS_SCHED_EVT_BRAND_ACTIVE, 1);
			//spd_mpd_ipc_release((struct composite_spd *)thd_get_thd_spdpoly(curr));
			cos_meas_event(COS_MEAS_BRAND_COMPLETION_PENDING);
			event_record("switch_thread tailcall pending", thd_get_id(curr), 0);
			report_upcall("c", curr);
			*ret_code = COS_SCHED_RET_SUCCESS;
			return sched_tailcall_pending_upcall_thd(curr, (struct composite_spd*)curr_spd->composite_spd);
		} 

		curr->flags &= ~THD_STATE_ACTIVE_UPCALL;
		curr->flags |= THD_STATE_READY_UPCALL;
		
		cos_meas_event(COS_MEAS_FINISHED_BRANDS);
		cos_meas_event(COS_MEAS_BRAND_COMPLETION_TAILCALL);

		*curr_flags = COS_SCHED_EVT_BRAND_READY;

		event_record("tailcall inv and switch to specified thread", thd_get_id(curr), thd_get_id(thd));
		report_upcall("f", curr);
	}

	/*** A synchronization event for the scheduler? ***/
	if (flags & COS_SCHED_SYNC_BLOCK) {
		union cos_synchronization_atom *l = &da->cos_locks;
		
		/* if a thread's version of which thread should be
		 * scheduled next does not comply with the in-memory
		 * version within the lock, then we are dealing with a
		 * stale invocation.
		 */
		if (l->c.owner_thd != next_thd) {
			cos_meas_event(COS_MEAS_ATOMIC_STALE_LOCK);
			*ret_code = COS_SCHED_RET_SUCCESS;
			goto ret_err;
		}
		cos_meas_event(COS_MEAS_ATOMIC_LOCK);

		/* This should only be set if it is the most urgent of
		 * the blocked threads waiting for owner_thd to
		 * complete.  I believe, though I haven't proven, that
		 * the most recent invocation of this syscall is this
		 * thread, so it is valid to simply set the queued_thd
		 * to the current one.
		 */
		l->c.queued_thd = thd_get_id(curr);
		/* 
		 * FIXME: alter the urgency/priority of the owner
		 * thread to inherit that of the current blocked thd.
		 */
	} else if (flags & COS_SCHED_SYNC_UNBLOCK) {
		cos_meas_event(COS_MEAS_ATOMIC_UNLOCK);
		/* 
		 * FIXME: reset urgency/priority of current thread back
		 * to natural state.
		 */
	}

	if (flags & COS_SCHED_CHILD_EVT) {
		struct thd_sched_info *tsi;
		struct spd *child;
		struct cos_sched_data_area *cda;

		tsi = thd_get_sched_info(thd, curr_spd->sched_depth+1);
		if (unlikely(!tsi || !tsi->scheduler)) goto_err(ret_err, "tsi fail, tid %d\n", thd_get_id(thd)); 

		/* If the scheduler exists, all of the following
		 * pointers should be non-NULL */
		child = tsi->scheduler;
		assert(child);
		cda = child->sched_shared_page[get_cpuid()];
		assert(cda);
		cda->cos_evt_notif.pending_cevt = 1;
	}

	return thd;
ret_err:
	return curr;
}

static inline void 
upcall_setup_regs(struct thread *uc, struct spd *dest,
		  upcall_type_t option, long arg1, long arg2, long arg3)
{
	struct pt_regs *r = &uc->regs;

	r->bx = arg1;
	r->di = arg2;
	r->si = arg3;
	r->cx = option;
	r->ip = r->dx = dest->upcall_entry;
	r->ax = thd_get_id(uc) | (get_cpuid() << 16);
}

/* 
 * Here we aren't throwing away the entire invocation stack.  Just add
 * the upcall frame on top of the existing stack.
 */
static struct thread *
upcall_inv_setup(struct thread *uc, struct spd *dest, upcall_type_t option,
		 long arg1, long arg2, long arg3)
{
	/* Call this first so that esp and eip are intact...clobbered
	 * in the next line */
	thd_invocation_push(uc, dest, uc->regs.sp, uc->regs.ip);
	upcall_setup_regs(uc, dest, option, arg1, arg2, arg3);
	spd_mpd_ipc_take((struct composite_spd *)dest->composite_spd);
	
	return uc;
}

static struct thread *
upcall_setup(struct thread *uc, struct spd *dest, upcall_type_t option,
	     long arg1, long arg2, long arg3)
{
	upcall_setup_regs(uc, dest, option, arg1, arg2, arg3);
	
	uc->stack_ptr = 0;
	uc->stack_base[0].current_composite_spd = dest->composite_spd;
	uc->stack_base[0].spd = dest;
	spd_mpd_ipc_take((struct composite_spd *)dest->composite_spd);

	return uc;
}

static inline struct thread *
upcall_execute(struct thread *uc, struct composite_spd *new,
	       struct thread *prev, struct composite_spd *old)
{
	if (prev && prev != uc) {
		/* This will switch the page tables for us */
		switch_thread_context(prev, uc);
	} else if (old != new) {
		/* we have already released the old->composite_spd */
		open_close_spd(&new->spd_info, &old->spd_info);
	}

	return uc;
}

/* This version of the call is to be used when we wish to make an
 * upcall that won't immediately switch page tables */
static inline struct thread *
upcall_execute_no_vas_switch(struct thread *uc, struct thread *prev)
{
	if (likely(prev && prev != uc)) {
		struct spd_poly *nspd;

		nspd = thd_get_thd_spdpoly(uc);
		__switch_thread_context(prev, uc, NULL, nspd);

		/* we are omitting the native_write_cr3 to switch
		 * page tables */
		__chal_pgtbl_switch(nspd->pg_tbl);
	}
	return uc;
}

static inline struct thread *
upcall_execute_compat(struct thread *uc, struct thread *prev, 
		      struct composite_spd *old)
{
	struct composite_spd *cspd = (struct composite_spd*)uc->stack_base[0].current_composite_spd;
	return upcall_execute(uc, cspd, prev, old);
}

static struct thd_sched_info *
scheduler_find_leaf(struct thread *t)
{
	struct thd_sched_info *tsi = NULL, *prev_tsi;
	int i = 0;

	/* find the child scheduler */
	do {
		prev_tsi = tsi;
		if (i == MAX_SCHED_HIER_DEPTH) break;
		tsi = thd_get_sched_info(t, i);
		i++;
	} while (tsi->scheduler);

	return prev_tsi;
}

static inline struct pt_regs *
thd_ret_upcall_type(struct thread *curr, upcall_type_t t)
{
	struct composite_spd *cspd = (struct composite_spd *)thd_get_thd_spdpoly(curr);
	struct thd_sched_info *tsi;
	struct spd *dest;
	assert(cspd);
	spd_mpd_ipc_release(cspd);
	
	tsi = scheduler_find_leaf(curr);
	dest = tsi->scheduler;
	
	upcall_setup(curr, dest, t, 0, 0, 0);
	upcall_execute_compat(curr, NULL, cspd);
	
	return &curr->regs;
}

/* Upcall into base scheduler! */
static struct pt_regs *
thd_ret_term_upcall(struct thread *curr)
{
	return thd_ret_upcall_type(curr, COS_UPCALL_DESTROY);
}

//static int cos_net_try_packet(struct thread *brand, unsigned short int *port);

static struct thread *
sched_tailcall_pending_upcall_thd(struct thread *uc, struct composite_spd *curr)
{
	struct thread *brand = uc->thread_brand;
	struct composite_spd *cspd;

	assert(brand && brand->pending_upcall_requests > 0);
	assert(uc->flags & THD_STATE_ACTIVE_UPCALL && 
	       !(uc->flags & THD_STATE_READY_UPCALL));

	brand->pending_upcall_requests--;

	cspd = (struct composite_spd*)thd_get_thd_spdpoly(uc);
	upcall_execute(uc, cspd, NULL, curr);

	cos_meas_event(COS_MEAS_BRAND_PEND_EXECUTE);
	cos_meas_event(COS_MEAS_FINISHED_BRANDS);

	/* return value is the number of pending upcalls */
	uc->regs.ax = 0;//brand->pending_upcall_requests;

	event_record("pending upcall", thd_get_id(uc), 0);

	return uc;
}

/* 
 * Assumes: we are called from the thread switching syscall, with the
 * TAIL_CALL flag (i.e. we are switching away from an upcall).  Also,
 * that the previous component was released.
 */
static struct pt_regs *
sched_tailcall_pending_upcall(struct thread *uc, struct composite_spd *curr)
{
	return &sched_tailcall_pending_upcall_thd(uc, curr)->regs;
}

/* 
 * If a brand has completed and there are no pending brand
 * activations, and we wish to switch to another thread, this function
 * should do the job.
 */
static void brand_completion_switch_to(struct thread *curr, struct thread *prev)
{
	/* 
	 * From here on, we know that we have an interrupted thread
	 * that we are returning to.
	 */
	cos_meas_event(COS_MEAS_BRAND_SCHED_PREEMPTED);
	cos_meas_event(COS_MEAS_FINISHED_BRANDS);

	break_preemption_chain(curr);

	curr->flags &= ~THD_STATE_ACTIVE_UPCALL;
	curr->flags |= THD_STATE_READY_UPCALL;
	/* 
	 * FIXME: this should be more complicated.  If
	 * a scheduling decision has been made between
	 * when the upcall thread was scheduled, and
	 * now.  In such a case, the "previous
	 * preempted thread" could have already
	 * executed to completion, or some such.  In
	 * such a case (scheduling decision has been
	 * made to put the upcall thread to sleep),
	 * then the correct thing to do is to act like
	 * this thread has been killed (or yields, or
	 * something in between) for scheduling
	 * purposes (assuming that we don't have
	 * pending upcalls, which changes all of this.
	 *
	 * UPDATE: this has been dealt with by adding the
	 * BREAK_PREEMPTION_CHAIN flag to sched_cntl.
	 */
	switch_thread_context(curr, prev);

	/* This might not be true if we are a brand that was just
	 * branded by another thread.  That other thread just branded
	 * us, and wasn't preempted. */
	if (prev->flags & THD_STATE_PREEMPTED) {
		remove_preempted_status(prev);
	}
	update_sched_evts(prev, COS_SCHED_EVT_NIL, 
			  curr, COS_SCHED_EVT_BRAND_READY);
}

static struct pt_regs *brand_execution_completion(struct thread *curr, int *preempt)
{
	struct thread *prev, *brand = curr->thread_brand;
    	struct composite_spd *cspd = (struct composite_spd *)thd_get_thd_spdpoly(curr);
	
	assert((curr->flags & THD_STATE_ACTIVE_UPCALL) &&
	       !(curr->flags & THD_STATE_READY_UPCALL));
	assert(brand && cspd);

	cos_meas_stats_end(COS_MEAS_STATS_UC_TERM_DELAY, 1);
	cos_meas_stats_end(COS_MEAS_STATS_UC_PEND_DELAY, 0);
	*preempt = 0;

	/* Immediately execute a pending upcall */
	if (brand->pending_upcall_requests) {
		event_record("brand complete, self pending upcall executed", thd_get_id(curr), 0);
		report_upcall("c", curr);
		return sched_tailcall_pending_upcall(curr, cspd);
	}

	/*
	 * Has the thread we preempted had scheduling activity since?
	 * If so, upcall into the root scheduler and ask it what to
	 * do.
	 */
	prev = curr->interrupted_thread;

	if (NULL == prev) {
		struct thd_sched_info *tsi, *prev_tsi;
		struct spd *dest;
		int i;
	
		prev_tsi = thd_get_sched_info(brand, 0);
		assert(prev_tsi->scheduler);

		for (i = 1 ; i < MAX_SCHED_HIER_DEPTH ; i++) {
			//tsi = scheduler_find_leaf(curr);
			tsi = thd_get_sched_info(brand, i);
			if (!tsi->scheduler) break;
			prev_tsi = tsi;
		}
		tsi = prev_tsi;
		assert(tsi);
		dest = tsi->scheduler;

		upcall_inv_setup(curr, dest, COS_UPCALL_BRAND_COMPLETE, 0, 0, 0);
		upcall_execute(curr, (struct composite_spd*)dest->composite_spd, 
			       NULL, cspd);

		event_record("brand complete, upcall scheduler", thd_get_id(curr), 0);

		cos_meas_event(COS_MEAS_BRAND_COMPLETION_UC);
		//cos_meas_event(COS_MEAS_FINISHED_BRANDS);
		return &curr->regs;
	}

	event_record("brand completion, switch to interrupted thread", thd_get_id(curr), thd_get_id(prev));
	brand_completion_switch_to(curr, prev);
	*preempt = 1;
	report_upcall("i", curr);

	return &prev->regs;
}

/**
 * Upcall interfaces:
 *
 * The current interfaces for upcalls rely on maintaining the state of
 * an execution path through components which is traversed in reverse
 * order by each upcall.  This mechanism will not work when components
 * are collapsed into larger protection domains as 1) the direct
 * function calls will not know where to execute (as the invocation
 * stack is kept in kernel) and 2) the kernel will not know in which
 * component an invocation is made from.
 *
 * When a thread is going to request a brand, it must make only
 * invocations via SDT (and will therefore not be on a fast-path).
 * When we are making brand upcalls, we might skip some spds in the
 * invocation stack of the brand thread due to MPD whereby multiple
 * spds are collapsed into one composite spd.
 */
struct thread *brand_next_thread(struct thread *brand, struct thread *preempted, int preempt);

//#define BRAND_UL_LATENCY
extern void cos_syscall_brand_wait(int spd_id, unsigned short int bid, int *preempt);
COS_SYSCALL struct pt_regs *
cos_syscall_brand_wait_cont(int spd_id, unsigned short int bid, int *preempt)
{
	struct thread *curr, *brand;
	struct spd *curr_spd;

	curr = core_get_curr_thd();

	curr_spd = thd_validate_get_current_spd(curr, spd_id);
	if (unlikely(NULL == curr_spd)) {
		printk("cos: component claimed in spd %d, but not\n", spd_id);
		goto brand_wait_err;		
	}
	brand = thd_get_by_id(bid);
	if (unlikely(NULL == brand)) {
		printk("cos: Attempting to wait for brand thd %d - invalid thread.\n", bid);
		goto brand_wait_err;
	}
	if (unlikely(brand != curr->thread_brand)) {
		printk("cos: specified brand %d not one associated with %d\n", bid, thd_get_id(curr));
		goto brand_wait_err;
	}
	if (unlikely(curr_spd != thd_invstk_top(brand)->spd)) {
		printk("cos: spd that wishes to brand_wait, not one brand is registered in. Brand %d, bspd %d, curr %d, cspd %d\n",
		       thd_get_id(brand), spd_get_index(thd_invstk_top(brand)->spd), thd_get_id(curr), spd_get_index(curr_spd));
		goto brand_wait_err;
	}

	return brand_execution_completion(curr, preempt);
brand_wait_err:
	curr->regs.ax = -1;
	return &curr->regs;
}

extern void cos_syscall_brand_upcall(int spd_id, int thread_id_flags);
COS_SYSCALL struct pt_regs *
cos_syscall_brand_upcall_cont(int spd_id, int thread_id_flags, int arg1, int arg2)
{
	struct thread *curr_thd, *brand_thd, *next_thd;
	struct spd *curr_spd;
	short int thread_id, flags;

//	static int first = 1;

	thread_id = thread_id_flags>>16;
	flags = thread_id_flags & 0x0000FFFF;
	curr_thd = core_get_curr_thd();

	curr_spd = thd_validate_get_current_spd(curr_thd, spd_id);
	if (unlikely(NULL == curr_spd)) {
		printk("cos: component claimed in spd %d, but not\n", spd_id);
		goto upcall_brand_err;		
	}
	/*
	 * TODO: Check that the brand thread is on the same cpu as the
	 * current thread.
	 */
	brand_thd = thd_get_by_id(thread_id);
	if (unlikely(NULL == brand_thd)) {
		printk("cos: Attempting to brand thd %d - invalid thread.\n", thread_id);
		goto upcall_brand_err;
	}
/*	if (unlikely(thd_get_thd_spd(brand_thd) != curr_spd)) {
		printk("cos: attempted to make brand on thd %d, but from incorrect spd.\n", thread_id);
		goto upcall_brand_err;
	}
*/
	if (unlikely(!(brand_thd->flags & THD_STATE_BRAND) || !brand_thd->upcall_threads)) {
		printk("cos: cos_brand_upcall, thread %d not a brand\n", thread_id);
		goto upcall_brand_err;
	}

	/*
	 * FIXME: 1) reference counting taken care of????, 2) return 1
	 * if pending invocation?
	 */

#ifdef BRAND_UL_LATENCY
	glob_hack_arg = arg1;
#endif
	next_thd = brand_next_thread(brand_thd, curr_thd, 2);
	
	if (next_thd == curr_thd) {
		curr_thd->regs.ax = 0;
	} else {
		next_thd->regs.bx = arg1;
		next_thd->regs.di = arg2;
		curr_thd->regs.ax = 1;
	}
/* This to measure the cost of pending upcalls
	if (unlikely(first)) {
		brand_thd->pending_upcall_requests = 10000000;
		first = 0;
	}
*/
	return &next_thd->regs;

upcall_brand_err:
	curr_thd->regs.ax = -1;
	return &curr_thd->regs;
}

/*
 * TODO: Creating the thread in this function is a little
 * brain-damaged because now we are allocating threads without the
 * scheduler knowing it (shouldn't be allowed), and because there is a
 * limited number of threads, we could denial of service and no policy
 * could be installed in the system to stop us.  Solution: allow the
 * scheduler to create threads that aren't executed, but attached to
 * other threads (that make them), and these threads can later be used
 * to create brands and upcalls.  This allows the scheduler to control
 * the distrubution of threads, and essentially is a resource credit
 * to a principal where the resource here is a thread.
 *
 * FIXME: the way we record and do brand paths is incorrect currently.
 * It will work now, but not when we activate MPDs.  We need to make
 * sure that 1) all spd invocations are recorded when we are creating
 * a brand path, and 2) pointers are added only to the spds
 * themselves, not necessarily the spd's current protection domains as
 * we wish, when making upcalls to upcall into the most recent version
 * of the spd's protection domains.  This begs the question, when we
 * upcall_brand from a composite spd, how does the system know which
 * spd we are in, thus which to upcall into.  Solution: we must make
 * the upcall call be another capability which we can define a
 * user-level-cap for.  This is required anyway, as we need to have a
 * system-provided lookup for direct invocation.  When an upcall is
 * made, we walk the invocation stack till we find the current spd,
 * and upcall its return spd.  To improve usability, we should check
 * explicitely that when a brand is made, the chain of invocations
 * follows capabilities and doesn't skip spds due to mpds.
 */
static inline struct thread* verify_brand_thd(unsigned short int thd_id)
{
	struct thread *brand_thd;
	
	brand_thd = thd_get_by_id(thd_id);
	if (brand_thd == NULL) {
		printk("cos: cos_syscall_brand_cntl could not find thd_id %d to add thd to.\n", 
		       (unsigned int)thd_id);
		return NULL;
	}
	if (!(brand_thd->flags & THD_STATE_BRAND ||
	      brand_thd->flags & THD_STATE_HW_BRAND)) {
		printk("cos: cos_brand_cntl - adding upcall thd to thd %d that's not a brand\n",
		       (unsigned int)thd_id);
		return NULL;
	}
	
	return brand_thd;
}

COS_SYSCALL int 
cos_syscall_brand_cntl(int spd_id, int op, u32_t bid_tid, spdid_t dest)
{
	u16_t bid, tid, retid;
	struct thread *new_thd, *curr_thd;
	struct spd *curr_spd;
	int flags = 0;

	bid = bid_tid >> 16;
	tid = bid_tid & 0xFFFF;

	curr_thd = core_get_curr_thd();
	curr_spd = thd_validate_get_current_spd(curr_thd, spd_id);
	if (NULL == curr_spd) {
		printk("cos: component claimed in spd %d, but not\n", spd_id);
		return -1;		
	}

	switch (op) {
	case COS_BRAND_CREATE_HW:
		flags = THD_STATE_HW_BRAND;
		/* fall through */
	case COS_BRAND_CREATE: 
	{
		int depth;
		struct spd *s;
		struct thd_invocation_frame *f;
		int clear = 0, i;

		new_thd = thd_alloc(curr_spd);
		if (NULL == new_thd) return -1;

		/* the brand thread holds the invocation stack record: */
		memcpy(&new_thd->stack_base, &curr_thd->stack_base, sizeof(curr_thd->stack_base));
		new_thd->cpu_id = curr_thd->cpu_id;
		new_thd->flags |= (THD_STATE_BRAND | flags);
		s = spd_get_by_index(dest);
		if (NULL == s) {
			printk("cos: brand_cntl -- spd %d not found\n", dest);
			thd_free(new_thd);
			return -1;
		}
		if (-1 == (depth = thd_validate_spd_in_callpath(curr_thd, s))) {
//		if (depth > curr_thd->stack_ptr) {
			printk("cos: brand_cntl -- spd %d not found in stack for thread %d\n",
			       dest, thd_get_id(curr_thd));
			thd_free(new_thd);
			return -1;
		}
		new_thd->stack_ptr = curr_thd->stack_ptr - depth;
		f = &new_thd->stack_base[new_thd->stack_ptr];
		assert(thd_spd_in_composite(f->current_composite_spd, s));
		/* HACK: brands made past the first entry spd will break. */
		f->spd = s;

		copy_sched_info(new_thd, curr_thd);
		for (i = 0 ; i < MAX_SCHED_HIER_DEPTH ; i++) {
			struct thd_sched_info *tsi;

			tsi = thd_get_sched_info(new_thd, i);
			if (clear == 1) {
				tsi->scheduler = NULL;
				tsi->thread_notifications = NULL;
				continue;
			}
			if (tsi->scheduler == curr_spd) clear = 1;
			//assert(tsi->scheduler);
		}
		new_thd->flags |= THD_STATE_CYC_CNT;
		
		retid = new_thd->thread_id;
		break;
	} 
	case COS_BRAND_ADD_THD:
	{
		struct thread *brand_thd = verify_brand_thd(bid);
		struct thread *t = thd_get_by_id(tid);

		if (NULL == t || NULL == brand_thd) return -1;
		if (NULL != t->thread_brand) return -1;
		if (NULL != brand_thd->upcall_threads) return -1;
		assert(!(t->flags & THD_STATE_UPCALL));

		t->flags |= (THD_STATE_UPCALL | THD_STATE_ACTIVE_UPCALL);
		t->thread_brand = brand_thd;
		t->upcall_threads = brand_thd->upcall_threads;
		brand_thd->upcall_threads = t;
		break_preemption_chain(t);
		t->flags |= THD_STATE_CYC_CNT;

		retid = t->thread_id;
		//print_thd_sched_structs(new_thd);
		break;
	}
	default:
		return -1;
	}

	return retid;
}

struct thread *cos_timer_brand_thd[NUM_CPU]; CACHE_ALIGNED
struct thread *cos_upcall_notif_thd[NUM_CPU]; CACHE_ALIGNED

#define NUM_NET_BRANDS 2
unsigned int active_net_brands = 0;
struct cos_brand_info cos_net_brand[NUM_NET_BRANDS];
struct cos_net_callbacks *cos_net_fns = NULL;

void cos_net_init(void)
{
	int i;
	
	active_net_brands = 0;
	for (i = 0 ; i < NUM_NET_BRANDS ; i++) {
		cos_net_brand[i].brand = NULL;
		cos_net_brand[i].brand_port = 0;
	}
}

struct cos_brand_info *cos_net_brand_info(struct thread *t)
{
	int i;

	for (i = 0 ; i < NUM_NET_BRANDS ; i++) {
		if (cos_net_brand[i].brand == t) {
			return &cos_net_brand[i];
		}
	}
	return NULL;
}

void cos_net_finish(void)
{
	int i;
	
	active_net_brands = 0;
	for (i = 0 ; i < NUM_NET_BRANDS ; i++) {
		if (cos_net_brand[i].brand) {
			if (!cos_net_fns || !cos_net_fns->remove_brand ||
			    cos_net_fns->remove_brand(&cos_net_brand[i])) {
				printk("cos: error deregistering net brand for port %d\n",
					cos_net_brand[i].brand_port);
			}
		}
		cos_net_brand[i].brand = NULL;
		cos_net_brand[i].brand_port = 0;
	}
}

void cos_net_register(struct cos_net_callbacks *cn_cb)
{
	assert(cn_cb->get_packet && cn_cb->create_brand);

	printk("cos: Registering networking callbacks @ %x\n", (unsigned int)cn_cb);
	cos_net_fns = cn_cb;
}

void cos_net_deregister(struct cos_net_callbacks *cn_cb)
{
	assert(cos_net_fns == cn_cb);

	printk("cos: Deregistering networking callbacks\n");
	cos_net_fns = NULL;
}

void cos_net_prebrand(void)
{
	cos_meas_event(COS_MEAS_PACKET_RECEPTION);
}

extern int rb_retrieve_buff(struct thread *brand, int desired_len, 
			    void **found_buf, int *found_len);
extern int rb_setup(struct thread *brand, ring_buff_t *user_rb, ring_buff_t *kern_rb);

int cos_net_try_brand(struct thread *t, void *data, int len)
{
	void *buff;
	int l;
	unsigned int *lenp;

	cos_meas_event(COS_MEAS_PACKET_BRAND);

	/* 
	 * If there is no room for the network buffer, then don't
	 * attempt the upcall.  This is analogous to not trying to
	 * raise an interrupt when there are no buffers to write into.
	 */
	if (rb_retrieve_buff(t, len + sizeof(unsigned int), &buff, &l)) {
//	if (rb_retrieve_buff(t, len, &buff, &l)) {
		cos_meas_event(COS_MEAS_PACKET_BRAND_FAIL);
		return -1;
	}
	cos_meas_event(COS_MEAS_PACKET_BRAND_SUCC);
	lenp = buff;
	buff = &lenp[1];
	*lenp = len;
	memcpy(buff, data, len);
	
	chal_attempt_brand(t);
	
	return 0;
}

int cos_net_notify_drop(struct thread *brand)
{
	struct thread *uc;

	if (!brand) return -1;

	uc = brand->upcall_threads;
	if (uc) {
/*		if (uc->flags & THD_STATE_READY_UPCALL) {
			cos_meas_event(COS_MEAS_PENDING_HACK)
		}
*/
		if (brand->pending_upcall_requests == 0) {
			cos_meas_event(COS_MEAS_PENDING_HACK);
		}
		//update_thd_evt_state(uc, COS_SCHED_EVT_BRAND_PEND, 1);
	} else {
		return -1;
	}

	return 0;
}

/****************************/
/*** Translator Interface ***/
/****************************/

static const struct cos_trans_fns *trans_fns = NULL;
void cos_trans_reg(const struct cos_trans_fns *fns) { trans_fns = fns; }
void cos_trans_dereg(void) { trans_fns = NULL; }
void cos_trans_upcall(void *brand) 
{
	assert(brand);
	chal_attempt_brand((struct thread *)brand);
}

COS_SYSCALL int
cos_syscall_trans_cntl(spdid_t spdid, unsigned long op_ch, unsigned long addr, int off)
{
	int op, channel;

	op = op_ch >> 16;
	channel = op_ch & 0xFFFF;

	switch (op) {
	case COS_TRANS_TRIGGER:
		if (trans_fns) return trans_fns->levt(channel);
	case COS_TRANS_MAP_SZ:
	{
		int sz = -1;
		if (trans_fns) sz = trans_fns->map_sz(channel);

		return sz;
	}
	case COS_TRANS_MAP:
	{
		unsigned long kaddr;
		int sz;
		struct spd *s;

		s = spd_get_by_index(spdid);
		if (!s) return -1;
		if (!trans_fns) return -1;
		kaddr = (unsigned long)trans_fns->map_kaddr(channel);
		if (!kaddr) return -1;
		sz    = trans_fns->map_sz(channel);
		if (off > sz) return -1;

		if (chal_pgtbl_add(s->spd_info.pg_tbl, addr, (paddr_t)chal_va2pa(((char *)kaddr+off)))) {
			printk("cos: trans grant -- could not add entry to page table.\n");
			return -1;
		}
		return 0;
	}
	case COS_TRANS_DIRECTION:
		if (trans_fns) return trans_fns->direction(channel);
	case COS_TRANS_BRAND:
	{
		int tid = addr;
		struct thread *t;
		
		t = thd_get_by_id(tid);
		if (!t) return -1;
		if (!trans_fns) return -1;
		if (trans_fns->brand_created(channel, t)) return -1;

		return 0;
	}
	}
	return -1;
}

/* 
 * Partially emulate a device here: Receive ring for holding buffers
 * to receive data into, and a synchronous call to transmit data.
 */
extern int user_struct_fits_on_page(unsigned long addr, unsigned int size);
/* assembly in ipc.S */
extern int cos_syscall_buff_mgmt(void);
COS_SYSCALL int 
cos_syscall_buff_mgmt_cont(int spd_id, void *addr, unsigned int thd_id, unsigned int len_op)
{
	/* 
	 * FIXME: To do this right, we would need to either 1) pin the
	 * buffer's pages into memory, or 2) interact closely with the
	 * network subsystem so that any memory pages the mem_man
	 * unmaps from the address space are removed from the network
	 * buffer lists too.  For 1, the pages might be pre-pinned at
	 * map time.
	 */
	struct spd *spd;
	vaddr_t kaddr = 0;
	unsigned short int option, len;

	option = (len_op & 0xFFFF);
	len = len_op >> 16;

	spd = thd_validate_get_current_spd(core_get_curr_thd(), spd_id);
	if (!spd) {
		printk("cos: buff mgmt -- invalid spd, %d for thd %d\n", 
		       spd_id, thd_get_id(core_get_curr_thd()));
		return -1;
	}

	if (unlikely(COS_BM_XMIT != option &&
		     0 == (kaddr = chal_pgtbl_vaddr2kaddr(spd->spd_info.pg_tbl, (unsigned long)addr)))) {
		printk("cos: buff mgmt -- could not find kernel address for %p in spd %d\n",
		       addr, spd_id);
		return -1;
	}
	
	switch(option) {
	/* Transmit the data buffer */
	case COS_BM_XMIT:
	{
		struct cos_net_xmit_headers *h = spd->cos_net_xmit_headers[get_cpuid()];
		int gather_buffs = 0, i, tot_len = 0;
		struct gather_item gi[XMIT_HEADERS_GATHER_LEN];

		if (unlikely(NULL == h)) return -1;
		gather_buffs = h->gather_len;
		if (unlikely(gather_buffs > XMIT_HEADERS_GATHER_LEN)) {
			printk("cos buff mgmt -- gather list length %d too large.", gather_buffs);
			return -1;
		}
		/* Check that each of the buffers in the gather list are legal */
		for (i = 0 ; i < gather_buffs ; i++) {
			struct gather_item *user_gi = &h->gather_list[i];
			tot_len += user_gi->len;

			if (unlikely(!user_struct_fits_on_page((unsigned long)user_gi->data, user_gi->len))) {
				printk("cos: buff mgmt -- buffer address  %p does not fit onto page\n", user_gi->data);
				return -1;
			}
			if ((void*)((unsigned int)(user_gi->data) & PAGE_MASK) == 
			    get_shared_data()->argument_region) {
				/* If the pointer is into the argument
				 * region, we now that the memory is
				 * pinned. */
				kaddr = (vaddr_t)user_gi->data;
			} else {
				kaddr = chal_pgtbl_vaddr2kaddr(spd->spd_info.pg_tbl, (unsigned long)user_gi->data);
				if (unlikely(!kaddr)) {		    
					printk("cos: buff mgmt -- could not find kernel address for %p in spd %d\n",
					       user_gi->data, spd_id);
					return -1;
				}
			}

			gi[i].data = (void*)kaddr;
			gi[i].len  = user_gi->len;
		}

		/* Transmit! */
		if (likely(cos_net_fns && cos_net_fns->xmit_packet && h)) {
			cos_meas_event(COS_MEAS_PACKET_XMIT);
			return cos_net_fns->xmit_packet(h->headers, h->len, gi, gather_buffs, tot_len);
		}
		break;
	}
	case COS_BM_XMIT_REGION:
	{
		if (len != sizeof(struct cos_net_xmit_headers)) {
			printk("cos: buff mgmt -- xmit header region of length %d, expected %d.\n",
			       len, sizeof(struct cos_net_xmit_headers));
			return -1;
		}
		if (!user_struct_fits_on_page((unsigned long)addr, len)) {
			printk("cos: buff mgmt -- xmit headers address %p w/ len %d does not fit onto page\n", 
			       addr, len);
			return -1;
		}
		/* FIXME: pin page in memory */
		spd->cos_net_xmit_headers[get_cpuid()] = (struct cos_net_xmit_headers*)kaddr;

		break;
	}
	/* Set the location of a user-level ring buffer */
	case COS_BM_RECV_RING:
	{
		struct thread *b;

		/*
		 * Currently, the ring buffer must be aligned on a
		 * page, and be a page long
		 */
		if ((unsigned long)addr & ~PAGE_MASK || len != PAGE_SIZE) {
			printk("cos: buff mgmt -- recv ring @ %p (%d) not on page boundary.\n", addr, len);
			return -1;
		}
		if (NULL == (b = thd_get_by_id(thd_id))) {
			printk("cos: buff mgmt could not find brand thd %d.\n", 
		       (unsigned int)thd_id);
			return -1;
		}
		if (b->flags & THD_STATE_UPCALL) {
			assert(b->thread_brand);
			b = b->thread_brand;
		}
		if (!(b->flags & THD_STATE_BRAND ||
		      b->flags & THD_STATE_HW_BRAND)) {
			printk("cos: buff mgmt attaching ring buffer to thread not a brand: %d\n",
			       (unsigned int)thd_id);
			return -1;
		}
/* needed?  Validate step done above...
		if (thd_get_thd_spd(b) != spd) {
			printk("cos: buff mgmt trying to set buffer for brand not in curr spd");
			return -1;
		}
*/
		/* FIXME: pin the page in memory. */
		if (rb_setup(b, (ring_buff_t*)addr, (ring_buff_t*)kaddr)) {
			printk("cos: buff mgmt -- could not setup the ring buffer.\n");
			return -1;
		}
		break;
	}
	default:
		printk("cos: buff mgmt -- unknown option %d.\n", option);
		return -1;
	}
	return 0;
}

extern void register_timers(void);
/*
 * This is a bandaid currently.  This syscall should really be 
 * replaced by something a little more subtle and more closely related
 * to the APIC and timer hardware, rather than the device in general.
 */
COS_SYSCALL int 
cos_syscall_brand_wire(int spd_id, int thd_id, int option, int data)
{
	struct thread *curr_thd, *brand_thd;
	struct spd *curr_spd;

	curr_thd = core_get_curr_thd();
	curr_spd = thd_validate_get_current_spd(curr_thd, spd_id);
	if (NULL == curr_spd) {
		printk("cos: wiring brand to hardware - component claimed in spd %d, but not\n", spd_id);
		return -1;		
	}

	brand_thd = verify_brand_thd(thd_id);
	if (NULL == brand_thd || !(brand_thd->flags & THD_STATE_HW_BRAND)) {
		printk("cos: wiring brand to hardware - thread %d not brand thd\n",
		       (unsigned int)thd_id);
		return -1;
	}

	switch (option) {
	case COS_HW_TIMER:
		register_timers();
		cos_timer_brand_thd[get_cpuid()] = brand_thd;
		
		break;
	case COS_HW_NET:
		if (active_net_brands >= NUM_NET_BRANDS || !cos_net_fns) {
			printk("cos: Too many network brands.\n\n");
			return -1;
		}

		cos_net_brand[active_net_brands].brand_port = (unsigned short int)data;
		cos_net_brand[active_net_brands].brand = brand_thd;
		if (!cos_net_fns ||
		    !cos_net_fns->create_brand || 
		    cos_net_fns->create_brand(&cos_net_brand[active_net_brands])) {
			printk("cos: could not create brand in networking subsystem\n");
			return -1;
		}
		active_net_brands++;

		break;
	case COS_UC_NOTIF:
		cos_upcall_notif_thd[get_cpuid()] = brand_thd;

		break;
	default:
		return -1;
	}

	return 0;
}

/*
 * verify that truster does in fact trust trustee
 */
static int verify_trust(struct spd *truster, struct spd *trustee)
{
	unsigned short int cap_no, max_cap, i;

	cap_no = truster->cap_base;
	max_cap = truster->cap_range + cap_no;

	for (i = cap_no ; i < max_cap ; i++) {
		if (invocation_capabilities[i].destination == trustee) {
			return 0;
		}
	}

	return -1;
}

/* 
 * I HATE this call...do away with it if possible.  But we need some
 * way to jump-start processes, let schedulers keep track of their
 * threads, and be notified when threads die.
 *
 * NOT performance sensitive: used to kick start spds and give them
 * active entities (threads).
 */
extern void cos_syscall_upcall(void);
COS_SYSCALL int 
cos_syscall_upcall_cont(int this_spd_id, int spd_id, struct pt_regs **regs)
{
	struct spd *dest, *curr_spd;
	struct thread *thd;

	assert(regs);
	*regs = NULL;

	dest = spd_get_by_index(spd_id);
	thd = core_get_curr_thd();
	curr_spd = thd_validate_get_current_spd(thd, this_spd_id);

	if (NULL == dest || NULL == curr_spd) {
		printk("cos: upcall attempt failed - dest_spd = %d, curr_spd = %d.\n",
		       dest     ? spd_get_index(dest)     : 0, 
		       curr_spd ? spd_get_index(curr_spd) : 0);
		return -1;
	}

	/*
	 * Check that we are upcalling into a service that explicitly
	 * trusts us (i.e. that the current spd is allowed to upcall
	 * into the destination.)
	 */
	if (verify_trust(dest, curr_spd) && curr_spd->sched_depth != 0) {
		printk("cos: upcall attempted from %d to %d without trust relation.\n",
		       spd_get_index(curr_spd), spd_get_index(dest));
		return -1;
	}

	open_close_spd(dest->composite_spd, curr_spd->composite_spd); 

	spd_mpd_ipc_release((struct composite_spd *)thd_get_thd_spdpoly(thd));//curr_spd->composite_spd);
	//spd_mpd_ipc_take((struct composite_spd *)dest->composite_spd);

	upcall_setup(thd, dest, COS_UPCALL_BOOTSTRAP, 0, 0, 0);
	*regs = &thd->regs;

	cos_meas_event(COS_MEAS_UPCALLS);

	return thd_get_id(thd) | get_cpuid() << 16;
}


/****************** begin event notification functions ******************/

/* 
 * Update the linked list in the shared data page between the kernel
 * and the scheduler as an event has been added.  It will attempt to
 * add the event to the end of the list.  If this event happened for a
 * thread that is already in that linked list, then don't modify the
 * list (to avoid cycles/trees).
 */
static int update_evt_list(struct thd_sched_info *tsi)
{
	unsigned short int prev_evt, this_evt;
	struct cos_sched_events *evts;
	struct spd *sched;
	struct cos_sched_data_area *da;
	
	assert(tsi);
	assert(tsi->scheduler);
	assert(tsi->scheduler->kern_sched_shared_page[get_cpuid()]);

	sched = tsi->scheduler;
	/* if tsi->scheduler, then all of this should follow */
	da = sched->kern_sched_shared_page[get_cpuid()];

	/* 
	 * Here we want to prevent a race condition:
	 *
	 * t1 executes through the scheduler and sets
	 * da->cos_next.next_thd_id to the next thread it believes it
	 * should schedule.  Then it is preempted before it can call
	 * switch_thread.  An event occurs (upcall, woken thread,
	 * etc.) which changes the scheduling decision.  t1 is run
	 * again, but it doesn't know about the event, so it switches
	 * to the wrong thread.  Prevent this by setting next_thd_id
	 * to 0 here, and check for that case in switch_thread.
	 */
	da->cos_next.next_thd_id = 0;
	/* same intention as previous line, but this deprecates the
	 * previous */
	da->cos_evt_notif.pending_event = 1;
			
	evts = da->cos_events;
	prev_evt = sched->prev_notification[get_cpuid()];
	this_evt = tsi->notification_offset;
	if (unlikely(prev_evt >= NUM_SCHED_EVTS ||
		     this_evt >= NUM_SCHED_EVTS ||
		     this_evt == 0)) {
		printk("cos: events %d and %d out of range!\n", prev_evt, this_evt);
		return -1;
	}

	/* so long as we haven't already processed this event, and it
	 * is not part of the linked list of events, then add it */
	if (prev_evt != this_evt && 
	    COS_SCHED_EVT_NEXT(&evts[this_evt]) == 0) {
		if (unlikely(COS_SCHED_EVT_NEXT(&evts[prev_evt]) != 0)) {
			printk("cos: user-level scheduler %d not following evt protocol for evt %d\n",
			       (unsigned int)spd_get_index(sched), (unsigned int)prev_evt);
			/*
			 * FIXME: how should we notify it?  Should we
			 * notify it?  What to do here?
			 */
		}
		COS_SCHED_EVT_NEXT(&evts[prev_evt]) = this_evt;
		sched->prev_notification[get_cpuid()] = this_evt;
//		printk(">>\tp = t\n");
	}

	return 0;
}

static inline void update_thd_evt_state(struct thread *t, int flags, unsigned long elapsed)
{
	int i;
	struct thd_sched_info *tsi;

	//assert(flags != COS_SCHED_EVT_NIL);

	for (i = 0 ; i < MAX_SCHED_HIER_DEPTH ; i++) {
		struct spd *sched;

		tsi = thd_get_sched_info(t, i);
		sched = tsi->scheduler;
		if (!sched) break;
		if (likely(tsi->thread_notifications)) {
			struct cos_sched_events *se = tsi->thread_notifications;
			u32_t p, n;

			switch(flags) {
			case COS_SCHED_EVT_BRAND_PEND:
				cos_meas_event(COS_MEAS_EVT_PENDING);
				break;
			case COS_SCHED_EVT_BRAND_READY:
				cos_meas_event(COS_MEAS_EVT_READY);
				break;
			case COS_SCHED_EVT_BRAND_ACTIVE:
				cos_meas_event(COS_MEAS_EVT_ACTIVE);
				break;
			}

			if (likely(elapsed)) {
				n = p = se->cpu_consumption;
				n += elapsed;
				 /* prevent overflow */
				if (unlikely(n < p)) se->cpu_consumption = ~0UL;
				else                 se->cpu_consumption = n;
			}

			/* 
			 * FIXME: should a pending flag update
			 * override an activate one????
			 */
			if (flags != COS_SCHED_EVT_NIL) {
				COS_SCHED_EVT_FLAGS(tsi->thread_notifications) = flags;
			}
			/* handle error conditions of list manip here??? */
			update_evt_list(tsi);
		}
	}
	
	return;
}

static void update_sched_evts(struct thread *new, int new_flags, 
			      struct thread *prev, int prev_flags)
{
	unsigned elapsed = 0;

	assert(new && prev);

	/* 
	 * - if either thread has cyc_cnt set, do rdtsc (this is
	 *   expensive, ~80 on P4 cycles, so avoid it if possible)
	 * - if prev has cyc_cnt set, do sched evt cycle update
	 * - if new_flags, do sched evt flags update on new
	 * - if prev_flags, do sched evt flags update on prev
	 */
	if (likely((new->flags | prev->flags) & THD_STATE_CYC_CNT)) {
		unsigned long last;

		last = cycle_cnt;
		rdtscl(cycle_cnt);
		elapsed = cycle_cnt - last;
	}
	
	if (new_flags != COS_SCHED_EVT_NIL) {
		update_thd_evt_state(new, new_flags, 0);
	}
	if (elapsed || prev_flags != COS_SCHED_EVT_NIL) {
		update_thd_evt_state(prev, prev_flags, elapsed);
	}

	return;
}

/****************** end event notification functions ******************/

/************** functions for parsing async set urgencies ************/

static inline int 
most_common_sched_depth(struct thread *t1, struct thread *t2)
{
	int i;

	/* root scheduler had better be the same */
	assert(thd_get_depth_sched(t1, 0) == thd_get_depth_sched(t2, 0));

	for (i = 1 ; i < MAX_SCHED_HIER_DEPTH ; i++) {
		struct spd *s1, *s2;

		s1 = thd_get_depth_sched(t1, i);
		s2 = thd_get_depth_sched(t2, i);

		/* If the scheduler's diverge, previous depth is most common */
		if (!s1 || s1 != s2) return i-1;
	}

	return MAX_SCHED_HIER_DEPTH-1;
}

/* 
 * Not happy with the complexity of this function...
 *
 * A thread has made a brand and wishes to execute an upcall.  Here we
 * decide if that upcall should be made now based on the currently
 * executing thread, or if it should be postponed for schedulers to
 * deal with later.
 * 
 * There are 4 sets of schedulers, a - the most common scheduler
 * between uc and preempted, >a - the set of schedulers of more
 * authority than a, <a_uc - the set of schedulers that own uc of less
 * authority than a, and <a_pre - the set of schedulers that own
 * preempted that have less authority than a.
 *
 * If upcall is already active, signal that it should be not run (ret 0)
 * otherwise notify all schedulers in (<a_uc + a + >a) that upcall has
 * awakened and
 * if the urgency of upcall in a is higher (lower numerically) than
 *    prev in a
 *    - check forall s in (<a_uc + a + >a) that upcall is not disabled.
 *    - return 1 to signal that upcall should be executed.
 */
int 
brand_higher_urgency(struct thread *upcall, struct thread *prev)
{
	int d;
	u16_t u_urg, p_urg;

	assert(upcall->thread_brand && upcall->flags & THD_STATE_UPCALL);

	d = most_common_sched_depth(upcall, prev);
	/* FIXME FIXME FIXME FIXME FIXME FIXME FIXME this is a stopgap
	 * measure.  I don't know hy these are null when we are
	 * shutting down the system but still get a packet.  This will
	 * shut it up for now.
	 */
	if (unlikely(!thd_get_sched_info(prev, d)->thread_notifications)) {
		if (!thd_get_sched_info(upcall, d)->thread_notifications) {
			printk("cos: skimping on brand metadata maintenance, and returning.\n");
			return 0;
		} else {
			/* upcall has the proper structure, prev doesn't! */
			return 1;
		}
	}
	u_urg = thd_get_depth_urg(upcall, d);
	p_urg = thd_get_depth_urg(prev, d);
	/* We should not run the upcall if it doesn't have more
	 * urgency, remember here that higher numerical values equals
	 * less importance. */
	if (u_urg < p_urg) {
		update_sched_evts(upcall, COS_SCHED_EVT_BRAND_ACTIVE, 
				  prev, COS_SCHED_EVT_NIL);
		return 1;
	} else {
		update_thd_evt_state(upcall, COS_SCHED_EVT_BRAND_ACTIVE, 1);
		return 0;
	}
}

/* 
 * This does NOT release the composite spd reference of the preempted
 * thread, as you might expect.
 *
 * preempt = 0 if you don't want any of the preemption lists to be
 * updated, and if you don't want the preempted thread to be set as
 * PREEMPTED.  Pass in 1 if you want both of those things.  2 if you
 * only want the lists to be updated.  I know...this needs to change:
 * hurried evolution.
 *
 * execution = 1 if you want this to possibly lead to the upcall being
 * executed.  Otherwise, it won't be, even if the schedulers deem it
 * to be most important.
 */
struct thread *
brand_next_thread(struct thread *brand, struct thread *preempted, int preempt)
{
	/* Assume here that we only have one upcall thread */
	struct thread *upcall = brand->upcall_threads;

	assert(brand->flags & (THD_STATE_BRAND|THD_STATE_HW_BRAND));
	assert(upcall && upcall->thread_brand == brand);

	/* 
	 * If the upcall is already active, the scheduler's already
	 * know what they're doing, and has chosen to run preempted.
	 * Don't second guess it.
	 *
	 * Do the same if upcall threads haven't been added to this
	 * brand.
	 */
	if (upcall->flags & THD_STATE_ACTIVE_UPCALL) {
		assert(!(upcall->flags & THD_STATE_READY_UPCALL));
		cos_meas_event(COS_MEAS_BRAND_PEND);
		cos_meas_stats_start(COS_MEAS_STATS_UC_PEND_DELAY, 0);
		/* FIXME: RACE. This could be running on more than one
		 * cores simultaneously. We need atomic increment. */
		brand->pending_upcall_requests++;

		event_record("brand activated, but upcalls active", thd_get_id(preempted), thd_get_id(upcall));
		/* 
		 * This is an annoying hack to make sure we notify the
		 * scheduler that the upcall is active.  Because
		 * upcall notifications are edge triggered, if for
		 * some reason the scheduler misses one of the
		 * notifications, this can server as a reminder.
		 */
//		update_thd_evt_state(upcall, COS_SCHED_EVT_BRAND_PEND, 1);
//		cos_meas_event(COS_MEAS_PENDING_HACK);
		report_upcall("p", upcall);

		return preempted;
	}

	assert(upcall->flags & THD_STATE_READY_UPCALL);

	upcall->flags |= THD_STATE_ACTIVE_UPCALL;
	upcall->flags &= ~THD_STATE_READY_UPCALL;

	cos_meas_stats_start(COS_MEAS_STATS_UC_EXEC_DELAY, 1);
	cos_meas_stats_start(COS_MEAS_STATS_UC_TERM_DELAY, 1);
	cos_meas_stats_start(COS_MEAS_STATS_UC_PEND_DELAY, 1);
	/* 
	 * Does the upcall have a higher urgency than the currently
	 * executing thread?
	 */
	if (brand_higher_urgency(upcall, preempted)) {
		if (unlikely(preempted->flags & THD_STATE_PREEMPTED)) {
			printk("cos: WTF - preempted thread %d preempted, upcall %d.\n", 
			       thd_get_id(preempted), thd_get_id(upcall));
			return preempted;
		}
		if (unlikely(preempted->preempter_thread != NULL)) {
			printk("cos: WTF - preempter thread pointer of preempted thread %d not null, upcall %d.\n",
			       thd_get_id(preempted), thd_get_id(upcall));
			return preempted;
		}
		if (likely(preempt)) {
			assert((preempted->flags & THD_STATE_PREEMPTED) == 0);
			assert(preempted->preempter_thread == NULL);

			/* 
			 * This dictates how the registers for
			 * preempted are restored later.
			 */
			if (preempt == 1) preempted->flags |= THD_STATE_PREEMPTED;
			preempted->preempter_thread = upcall;
			upcall->interrupted_thread = preempted;
		} else {
			upcall->interrupted_thread = NULL;
		}

		/* Actually setup the brand/upcall to happen here.
		 * If we aren't in the composite thread, be careful
		 * what state we change (e.g. page tables) */
		if (likely(chal_pgtbl_can_switch())) {
			upcall_execute(upcall, (struct composite_spd*)thd_get_thd_spdpoly(upcall),
				       preempted, (struct composite_spd*)thd_get_thd_spdpoly(preempted));
		} else {
			upcall_execute_no_vas_switch(upcall, preempted);
		}
#ifdef UPCALL_TIMING
		{
			u64_t t;
			struct spd *s;

			rdtscll(t);
			s = thd_get_thd_spd(upcall);
			if (s->sched_depth == 0) {
				struct cos_sched_data_area *da;
				
				da = s->kern_sched_shared_page[get_cpuid()];
				if (da) da->cos_evt_notif.timer = (u32_t)t;
			} else {
				if (-1 == (int)t) t = 0;
				upcall->regs.ax = (u32_t)t;
			}
		}
#else
		upcall->regs.ax = upcall->thread_brand->pending_upcall_requests;
#endif

		if (preempted->flags & THD_STATE_ACTIVE_UPCALL && upcall->interrupted_thread) {
			event_record("upcall activated and made immediately (preempted upcall)", 
				     thd_get_id(preempted), thd_get_id(upcall));
		} else if (upcall->interrupted_thread) {
			event_record("upcall activated and made immediately (w/ preempted thd)", 
				     thd_get_id(preempted), thd_get_id(upcall));
		} else {
			event_record("upcall activated and made immediately w/o preempted thd", 
				     thd_get_id(preempted), thd_get_id(upcall));
		}

		report_upcall("u", upcall);
		cos_meas_event(COS_MEAS_BRAND_UC);
		cos_meas_stats_end(COS_MEAS_STATS_UC_EXEC_DELAY, 1);
		return upcall;
	} 
		
	/* 
	 * If another upcall is what we attempted to preempt, we might
	 * have a higher priority than the thread that upcall had
	 * preempted.  Thus we must break its preemption chain.
	 */
	if (preempted->flags & THD_STATE_ACTIVE_UPCALL) break_preemption_chain(preempted);
	
	event_record("upcall not immediately executed (less urgent), continue previous thread", 
		     thd_get_id(preempted), thd_get_id(upcall));
//		printk("%d w\n", thd_get_id(upcall));

	report_upcall("d", upcall);

	cos_meas_event(COS_MEAS_BRAND_DELAYED);
	return preempted;
}

/************** end functions for parsing async set urgencies ************/

COS_SYSCALL int 
cos_syscall_sched_cntl(int spd_id, int operation, int thd_id, long option)
{
	struct thread *thd;
	struct spd *spd;

	thd = core_get_curr_thd();
	spd = thd_validate_get_current_spd(thd, spd_id);
	if (NULL == spd) {
		printk("cos: component claimed in spd %d, but not\n", spd_id);
		return -1;
	}

	if (spd->sched_depth < 0) {
		printk("cos: spd %d called sched_cntl, but not a scheduler.\n", spd_id);
		return -1;
	}
/* Is this necessary??
	tsi = thd_get_sched_info(thd, spd->sched_depth);
	if (tsi->scheduler != spd) {
		printk("cos: spd %d @ depth %d attempting sched_cntl not a scheduler of thd %d (%x != %x).\n",
		       spd_get_index(spd), spd->sched_depth, thd_get_id(thd), (unsigned int)tsi->scheduler, (unsigned int)spd);
		return -1;
	}
*/

	switch(operation) {
	case COS_SCHED_EVT_REGION:
	{
		unsigned long region = (unsigned long)option;

		if (region < spd->location[0].lowest_addr ||
		    region + PAGE_SIZE >= spd->location[0].lowest_addr + spd->location[0].size) {
			printk("cos: attempted evt region for spd %d @ %lx.\n", spd_get_index(spd), region);
			return -1;
		}
		spd->sched_shared_page[get_cpuid()] = (struct cos_sched_data_area *)region;
		/* We will need to access the shared_page for thread
		 * events when the pagetable for this spd is not
		 * mapped in.  */
		spd->kern_sched_shared_page[get_cpuid()] = (struct cos_sched_data_area *)
			chal_pgtbl_vaddr2kaddr(spd->spd_info.pg_tbl, region);
		spd->prev_notification[get_cpuid()] = 0;

		/* FIXME: pin the page */
		printk("core %u, sched shared region @%p, kern @%p\n", get_cpuid(), spd->sched_shared_page[get_cpuid()], spd->kern_sched_shared_page[get_cpuid()]);
		break;
	}
	case COS_SCHED_THD_EVT:
	{
		long idx = option;
		struct cos_sched_events *evts, *this_evt;
		struct thd_sched_info *tsi;
		struct thread *thd;
		
		thd = thd_get_by_id(thd_id);
		if (!thd) {
			printk("cos: thd id %d passed into register event %d invalid.\n",
			       (unsigned int)thd_id, (unsigned int)idx);
			return -1;
		}
		
		tsi = thd_get_sched_info(thd, spd->sched_depth);
		if (tsi->scheduler != spd) {
			printk("cos: spd %d not the scheduler of %d to associate evt %d.\n",
			       spd_get_index(spd), (unsigned int)thd_id, (unsigned int)idx);
			return -1;
		}

		if (idx >= NUM_SCHED_EVTS) {
			printk("cos: invalid thd evt index %d for scheduler %d\n", 
			       (unsigned int)idx, (unsigned int)spd_id);
			return -1;
		}

		if (0 == idx) {
			/* reset thread */
			evts = spd->kern_sched_shared_page[get_cpuid()]->cos_events;
			this_evt = &evts[idx];
			COS_SCHED_EVT_NEXT(this_evt) = 0;
			COS_SCHED_EVT_FLAGS(this_evt) = 0;
			this_evt->cpu_consumption = 0;

			tsi->thread_notifications = NULL;
			tsi->notification_offset = 0;
		} else {
			evts = spd->kern_sched_shared_page[get_cpuid()]->cos_events;
			this_evt = &evts[idx];
			tsi->thread_notifications = this_evt;
			tsi->notification_offset = idx;
			//COS_SCHED_EVT_NEXT(this_evt) = 0;
			//COS_SCHED_EVT_FLAGS(this_evt) = 0;
			//this_evt->cpu_consumption = 0;

			if (thd->flags & THD_STATE_BRAND) {
				struct thread *t = thd->upcall_threads;
				
				while (t) {
					copy_sched_info(t, thd);
					t = t->upcall_threads;
				}
			}
		}
		
		//print_thd_sched_structs(thd);
		break;
	}
	case COS_SCHED_PROMOTE_CHLD:
	{
		int sched_lvl = spd->sched_depth + 1;
		struct spd *child = spd_get_by_index((int)option);

		if (sched_lvl >= MAX_SCHED_HIER_DEPTH) {
			printk("Cannot promote child, exceeds sched hier depth.\n");
			return -1;
		}
		if (child->parent_sched && child->parent_sched != spd) {
			printk("Child scheduler already child to another scheduler.\n");
			return -1;
		}
		child->parent_sched = spd;
		child->sched_depth = sched_lvl;
		break;
	}
	case COS_SCHED_PROMOTE_ROOT:
		break;
	case COS_SCHED_GRANT_SCHED:
	case COS_SCHED_REVOKE_SCHED:
	{
		/*
		 * Permit a child scheduler the capability to schedule
		 * the thread, or remove that capability.  Assumes
		 * that 1) this spd is a scheduler that has the
		 * capability to schedule the thread, 2) the target
		 * spd is a scheduler that is a child of this
		 * scheduler.
		 */ 
		struct thd_sched_info *child_tsi;
		struct thread *target_thd = thd_get_by_id(thd_id);
		struct spd *child = spd_get_by_index((int)option);
		int i;
		
		if (NULL == target_thd         || 
		    NULL == child              || 
		    spd != child->parent_sched ||
		    !thd_scheduled_by(target_thd, spd)) {
			printk("cos: Could not give privs for sched %d to thd %d from sched %d.\n",
			       (unsigned int)option, (unsigned int)thd_id, (unsigned int)spd_id);
			return -1;
		}
		
		child_tsi = thd_get_sched_info(target_thd, child->sched_depth);

		if (COS_SCHED_GRANT_SCHED == operation) {
			child_tsi->scheduler = child;
		} else if (COS_SCHED_REVOKE_SCHED == operation) {
			if (child_tsi->scheduler != child) {
				printk("cos: cannot remove privs when they aren't had\n");
				return -1;
			}

			child_tsi->scheduler = NULL;
		}
		/*
		 * revoke all schedulers that are decendents of the
		 * child.
		 */
		for (i = child->sched_depth+1 ; i < MAX_SCHED_HIER_DEPTH ; i++) {
			child_tsi = thd_get_sched_info(target_thd, i);
			child_tsi->scheduler = NULL;
		}
	}
	case COS_SCHED_BREAK_PREEMPTION_CHAIN:
	{
		event_record("breaking preemption chain", thd_get_id(thd), 0);
		/* 
		 * This call is simple: make it so that when the
		 * current thread (presumably an upcall) completes,
		 * don't automatically switch to the preempted thread,
		 * instead make an upcall into the scheduler.
		 */
		break_preemption_chain(thd);
		break;
	}
	default:
		printk("cos: cos_sched_cntl illegal operation %d.\n", operation);
		return -1;
	}
	
	return 0;
}
	
/*
 * Assume spd \in cspd.  Remove spd from cspd and add it to new1. Add
 * all remaining spds in cspd to new2.  If new2 == NULL, and cspd's
 * refcnt == 1, then we can just remove spd from cspd, and use cspd as
 * the new2 composite spd.  Returns 0 if the two new composites are
 * populated, -1 if there is an error, and 1 if we instead just reuse
 * the composite passed in by removing the spd from it (requires, of
 * course that cspd ref_cnt == 1, so that its mappings can change
 * without effecting any threads).  This is common because when we
 * split and merge, we will create a composite the first time for the
 * shrinking composite, but because it won't have active threads, that
 * composite can simply be reused by removing any further spds split
 * from it.
 */
static int 
mpd_split_composite_populate(struct composite_spd *new1, struct composite_spd *new2, 
					struct spd *spd, struct composite_spd *cspd)
{
	struct spd *curr;
	int remove_mappings;

	assert(cspd && spd_is_composite(&cspd->spd_info));
	assert(new1 && spd_is_composite(&new1->spd_info));
	assert(spd && spd_is_member(spd, cspd));
	assert(new1 != cspd);

	remove_mappings = (NULL == new2);
	spd_composite_remove_member(spd, remove_mappings);

	if (spd_composite_add_member(new1, spd)) {
		printk("cos: could not add member to new spd in split.\n");
		goto err_adding;
	}

	/* If the cspd is updated by removing the spd, and that spd
	 * has been added to the new composite spd, new1, we're
	 * done */
	if (NULL == new2) {
		assert(cos_ref_val(&cspd->spd_info.ref_cnt) == 1);
		return 1;
	}

	/* aliasing will mess everything up here */
	assert(new1 != new2);

	while (cspd->members) {
		curr = cspd->members;
		if (spd_composite_move_member(new2, curr, 0)) {
			printk("cos: could not add spd to new composite in split.\n");
			goto err_adding;
		}
		assert(cspd->members != curr);
	}

	return 0;
 err_adding:
	return -1;
}

/*
 * Given a composite spd, c and a spd, s within it, split the spd out
 * of the composite, making two composite spds, c1 and c2.  c =
 * union(c1, c2), c\{s} = c1, {s} = c2.  This will create two
 * composite spds (c1 and c2) and will depricate and release (possibly
 * free) the preexisting composite c.  This method will reset all
 * capabilities correctly.
 */
static int 
mpd_split(struct composite_spd *cspd, struct spd *spd, short int *new, short int *old)
{
	short int d1, d2;
	struct composite_spd *new1, *new2;
	int ret = -1;

	assert(!spd_mpd_is_depricated(cspd));
	assert(spd_is_composite(&cspd->spd_info));
	assert(spd_is_member(spd, cspd));
	assert(spd_composite_num_members(cspd) > 1);

	d1 = spd_alloc_mpd_desc();
	if (d1 < 0) {
		printk("cos: could not allocate first mpd descriptor for split operation.\n");
		goto end;
	}
	new1 = spd_mpd_by_idx(d1);

	/*
	 * This condition represents the optimization whereby we wish
	 * to reuse the cspd instead of making a new one.  See the
	 * comment above mpd_composite_populate.  If we can reuse the
	 * current cspd by shrinking it rather than deleting it and
	 * allocating a new composite, then do it.
	 *
	 * This is a common case, e.g. when continuously moving spds
	 * from one cspd to another, but not in many other cases.
	 *
	 * It might be very possible to do this optimization when the
	 * reference count is > 1 because there is one subordinated
	 * domain, but that is the only (additional reference).
	 * Probably not worth adding in the logic for this.
	 */
	if (1 == cos_ref_val(&cspd->spd_info.ref_cnt)) {
		if (mpd_split_composite_populate(new1, NULL, spd, cspd) != 1) {
			ret = -1;
			goto err_d2;
		}
		*new = d1;
		*old = spd_mpd_index(cspd);

		cos_meas_event(COS_MPD_SPLIT_REUSE);

		ret = 0;
		goto end;
	}

	/* ...otherwise, we must allocate the other composite spd, and
	 * populate both of them */
	d2 = spd_alloc_mpd_desc();
	if (d2 < 0) {
		printk("cos: could not allocate second mpd descriptor for split operation.\n");
		goto err_d2;
	}
	new2 = spd_mpd_by_idx(d2);
	assert(new1 && new2);
	
	if (mpd_split_composite_populate(new1, new2, spd, cspd)) {
		printk("cos: populating two new cspds failed while splitting.\n");
		goto err_adding;
	}
	*new = d1;
	*old = d2;
	/* depricate the composite spd so that it cannot be used
	 * anymore from any user-level interfaces */
	spd_mpd_depricate(cspd);
	assert(!spd_mpd_is_depricated(new1) && !spd_mpd_is_depricated(new2));
	ret = 0;

	goto end;
	
 err_adding:
	spd_mpd_depricate(new2);
 err_d2:
	spd_mpd_depricate(new1);
 end:
	return ret;
}

/*
 * We want to subordinate one of the composite spds _only if_ we
 * cannot immediately free both the page tables (which subordination
 * does), but also the composite spd.  Thus, if one of the composites
 * can be freed, send it to be subordinated as it will be freed
 * immediately.  Secondarily, return the composite with fewer
 * components in it so there are less components to iterate through
 * when adding their mappings to the other composite.
 *
 * Let me make it explicit: First we are trying to save memory, then
 * processing time.  This might not be the appropriate long-term
 * prioritization, but only empirical studies will show.
 */
static inline struct composite_spd *
get_spd_to_subordinate(struct composite_spd *c1, struct composite_spd *c2)
{
	int members1, members2;

	if (1 == cos_ref_val(&c1->spd_info.ref_cnt) &&
	    1 != cos_ref_val(&c2->spd_info.ref_cnt)) return c1;

	members1 = spd_composite_num_members(c1);
	members2 = spd_composite_num_members(c2);
	assert(members1 > 0 && members2 > 0);
	if (members1 < members2) return c1;
	return c2;
}

/*
 * Move all of the components from other to dest, thus merging the two
 * composite spds into one: dest.  This includes adding to the pg_tbl
 * of dest, all components in other.  The first version of this
 * function will have us actually releasing the other cspd to be
 * collected when not in use anymore.  This can result in the page
 * table of other sticking around for possibly a long time.  The
 * second version (calling spd_mpd_make_subordinate) will deallocate
 * other's page table and make it use dest's page table.  A lifetime
 * constraint is added whereby the dest cspd cannot be deallocated
 * before other.  This is done with the reference counting mechanism
 * already present.  Other could really be deallocated now without
 * worrying about the page-tables, except that references to it can
 * still be held by threads making invocations.  If these threads
 * could be pointed to dest instead of other, we could deallocate
 * other even earlier.  Perhaps version three of this will change the
 * ipc return path and if the cspd to return to is subordinate, return
 * to the subordinate's master instead, decrimenting the refcnt on the
 * subordinate.
 *
 * Assume that the composites passed in aren't the same.
 */
static struct composite_spd *
mpd_merge(struct composite_spd *c1, struct composite_spd *c2)
{
	struct spd *curr;
	struct composite_spd *dest, *other;

	assert(NULL != c1 && NULL != c2);
	assert(spd_is_composite(&c1->spd_info) && spd_is_composite(&c2->spd_info));
	assert(!spd_mpd_is_depricated(c1) && !spd_mpd_is_depricated(c2));
	assert(!spd_mpd_is_subordinate(c1) && !spd_mpd_is_subordinate(c2));
	assert(c1 != c2);
	other = get_spd_to_subordinate(c1, c2);
	dest = (other == c1) ? c2 : c1;

	/* 
	 * While there are spds in the current composite, move them to
	 * the new composite.
	 */
	while (other->members) {
		curr = other->members;
		if (spd_composite_move_member(dest, curr, 1)) {
			assert(0);
			/* FIXME: should back out all those that were
			 * already transferred from one to the
			 * other...but this error is really only
			 * indicatory of an error in the kernel
			 * anyway. */
			printk("cos: could not move spd from one composite spd to another in the merge operation.\n");
			return NULL;
		}
		assert(other->members != curr);
	}
	//spd_mpd_depricate(other);
	/* 
	 * Now the subordinate cspd is only referenced (if at all) by
	 * invocation stack references and the singular "active"
	 * reference to be removed in the next call.
	 */
	spd_mpd_make_subordinate(dest, other);
	assert(!spd_mpd_is_depricated(dest) && !spd_mpd_is_subordinate(dest));
	//print_valid_pgtbl_entries(dest->spd_info.pg_tbl);

	return dest;
}

/* 
 * Here composite_spd and composite_dest are specified as normal spds,
 * and the meaning here is "the composite protection domain that this
 * spd is part of".
 */
COS_SYSCALL int 
cos_syscall_mpd_cntl(int spd_id, int operation, 
				     spdid_t spd1, spdid_t spd2)
{
	int ret = 0; 
	struct composite_spd *prev = NULL;
	struct spd *from = NULL;
	paddr_t curr_pg_tbl, new_pg_tbl;
	struct spd_poly *curr;
	struct thread *thd;

	if (spd1) {
		from = spd_get_by_index(spd1);
		if (0 == from) {
			printk("cos: mpd_cntl -- first composite spd %d not valid\n", spd1);
			return -1;
		}
		prev = (struct composite_spd *)from->composite_spd;
		assert(prev);
		assert(spd_is_composite(&prev->spd_info));
		assert(!spd_mpd_is_subordinate(prev) && !spd_mpd_is_depricated(prev));
	} 

	thd = core_get_curr_thd();
	assert(thd);
	curr = thd_get_thd_spdpoly(thd);
	/* keep track of this, as it might change during the course of this call */
	curr_pg_tbl = curr->pg_tbl;

	switch(operation) {
	case COS_MPD_SPLIT:
	{
		struct spd *transitory;
		struct composite_spd *trans_cspd;
		struct mpd_split_ret sret;

		if (NULL == prev) {
			printk("cos: mpd_cntl -- first composite spd %d not valid\n", spd1);
			ret = -1;
			break;
		}
		transitory = spd_get_by_index(spd2);
		if (NULL == transitory) {
			printk("cos: mpd_cntl -- failed to access normal spd (%d) for call to split.\n", spd2);
			ret = -1;
			break;
		}
		trans_cspd = (struct composite_spd *)transitory->composite_spd;
		assert(spd_is_composite(&trans_cspd->spd_info));
		assert(!spd_mpd_is_depricated(trans_cspd) && !spd_mpd_is_subordinate(trans_cspd));
		assert(spd_composite_num_members(prev) > 0);
		if (trans_cspd != prev) {
			printk("cos: mpd_cntl -- spd %d not in claimed composite for %d\n", spd2, spd1);
			return -1;
		}
		/*
		 * It is not correct to split a spd out of a composite
		 * spd that only contains one spd.  It is pointless,
		 * and should not be encouraged.
		 */
		if (spd_composite_num_members(prev) == 1) {
			ret = -1;
			break;
		}
		cos_meas_event(COS_MEAS_MPD_SPLIT);
//		printk("cos: split spd with cspd %p from %p, spdid %d\n", trans_cspd, prev, spd1);
		ret = mpd_split(prev, transitory, &sret.new, &sret.old);
		/* simply return 0 for success */
/* 		if (!ret) { */
/* 			/\*  */
/* 			 * Pack the two short indexes of the mpds into */
/* 			 * a single int, and return that. */
/* 			 *\/ */
/* 			ret = *((int*)&sret); */
/* 		} */

		break;
	}
	case COS_MPD_MERGE:
	{
		struct spd *second;
		struct composite_spd *other, *cspd_ret;
		
		if (NULL == prev) {
			printk("cos: mpd_cntl -- first composite spd %d not valid\n", spd1);
			ret = -1;
			break;
		}
		second = spd_get_by_index(spd2);
		if (0 == second) {
			printk("cos; mpd_cntl -- second composite spd %d invalid\n", spd2);
			ret = -1;
			break;
		}
		other = (struct composite_spd *)second->composite_spd;
		assert(spd_is_composite(&other->spd_info));
		assert(NULL != other && !spd_mpd_is_depricated(other) && !spd_mpd_is_subordinate(other));
//		printk("cos: merge %p(%d) and %p(%d)\n", prev, spd1, other, spd2);
		if (prev == other) {
//			printk("cos: skipping merge\n");
			ret = 0;
			break;
		}
		if (NULL == (cspd_ret = mpd_merge(prev, other))) {
			ret = -1;
			break;
		}
		cos_meas_event(COS_MEAS_MPD_MERGE);
		ret = 0;
		//ret = spd_mpd_index(cspd_ret);
		break;
	}
	case COS_MPD_UPDATE:
	{
		struct thd_invocation_frame *tif;
		struct spd *spd;

		struct spd_poly *poly;
		struct composite_spd *active_cspd, *curr_cspd;

		spd = thd_validate_get_current_spd(thd, spd_id);
		if (NULL == spd) {
			ret = -1;
			break;
		}
		tif = thd_invstk_top(thd);
		assert(tif);
		/* Common case: We are not in the entry point to the
		 * current protection domain -- can't update */
		if (likely(tif->spd != spd)) break;

		/* 
		 * If the currently active cspd (to be found in the
		 * invocation stack) is either 1) depricated, or 2)
		 * subordinate to a depricated cspd, then we wish to
		 * update the currently active spd to the most up to
		 * date configuration.
		 */
		poly = tif->current_composite_spd;
		active_cspd = (struct composite_spd *)poly;
		if (!(spd_mpd_is_depricated(active_cspd) ||
		      (spd_mpd_is_subordinate(active_cspd) && 
		       spd_mpd_is_depricated(active_cspd->master_spd)))) break;

		assert(poly->flags & SPD_COMPOSITE);
		/* We know we are currently in a depricated composite
		 * spd, but also that we can update to the current
		 * cspd...do it! */
		curr_cspd = (struct composite_spd *)spd->composite_spd;
		spd_mpd_ipc_take(curr_cspd);
		tif->current_composite_spd = (struct spd_poly*)curr_cspd;
		spd_mpd_ipc_release(active_cspd);

		break;
	}
	default:
		ret = -1;
	}

	curr = thd_get_thd_spdpoly(thd);
	assert(curr);
	new_pg_tbl = curr->pg_tbl;
	/*
	 * The page tables of the current spd can change if the
	 * current spd is subordinated to another spd.  If they did,
	 * we should do something about it:
	 */
	switch_pgtbls(new_pg_tbl, curr_pg_tbl);
	
	return ret;
}

/*
 * Well look at that:  full support for mapping in 50 lines of code.
 *
 * FIXME: check that 1) spdid is allowed to map, and 2) check flags if
 * the spd wishes to confirm that the dspd is in the invocation stack,
 * or is in the current composite spd, or is a child of a fault
 * thread.
 */
COS_SYSCALL int 
cos_syscall_mmap_cntl(int spdid, long op_flags_dspd, vaddr_t daddr, unsigned long mem_id)
{
	short int op, flags, dspd_id;
	paddr_t page;
	int ret = 0;
	struct spd *spd, *this_spd;
	
	/* decode arguments */
	op       = op_flags_dspd>>24;
	flags    = op_flags_dspd>>16 & 0x000000FF;
	dspd_id  = op_flags_dspd & 0x0000FFFF;
	this_spd = spd_get_by_index(spdid);
	spd      = spd_get_by_index(dspd_id);
	if (!this_spd || !spd || virtual_namespace_query(daddr) != spd) {
		printk("cos: invalid mmap cntl call for spd %d for spd %d @ vaddr %x\n",
		       spdid, dspd_id, (unsigned int)daddr);
		return -1;
	}

	switch(op) {
	case COS_MMAP_GRANT:
		mem_id += this_spd->pfn_base;
		if (mem_id < this_spd->pfn_base || /* <- check for overflow? */
		    mem_id >= (this_spd->pfn_base + this_spd->pfn_extent)) {
			printk("Accessing physical frame outside of allowed range (%d outside of [%d, %d).\n",
			       (int)mem_id, this_spd->pfn_base, 
			       this_spd->pfn_base + this_spd->pfn_extent);
			return -EINVAL;
		}
		page = cos_access_page(mem_id);
		if (0 == page) {
			printk("cos: mmap grant -- could not get a physical page.\n");
			return -EINVAL;
		}
		/*
		 * Demand paging could mess this up as the entry might
		 * not be in the page table, and we map in our cos
		 * page.  Ignore for the time being, as our loader
		 * forces demand paging to not be used (explicitly
		 * writing all of the pages itself).
		 */
		if (chal_pgtbl_add(spd->spd_info.pg_tbl, daddr, page)) {
			printk("cos: mmap grant into %d @ %x -- could not add entry to page table.\n", 
			       dspd_id, (unsigned int)daddr);
			ret = -1;
			break;
		}
		cos_meas_event(COS_MAP_GRANT);
		break;
	case COS_MMAP_REVOKE:
	{
		paddr_t pa;

		if (!(pa = chal_pgtbl_rem(spd->spd_info.pg_tbl, daddr))) {
			ret = 0;
			break;
		}
		ret = cos_paddr_to_cap(pa) - this_spd->pfn_base;
		cos_meas_event(COS_MAP_REVOKE);

		break;
	}
	case COS_MMAP_TLBFLUSH:
		chal_pgtbl_switch(spd->spd_info.pg_tbl);
		break;
	default:
		ret = -1;
	}

	return ret;
}

COS_SYSCALL int 
cos_syscall_pfn_cntl(int spdid, long op_dspd, unsigned int mem_id, int extent)
{
	struct spd *spd, *dspd;
	spdid_t dspdid;
	unsigned int end;
	int op, ret = 0;

	op     = op_dspd >> 16;
	dspdid = 0xFFFF & op_dspd;
	spd    = spd_get_by_index(spdid);
	dspd   = spd_get_by_index(dspdid);

	if (!spd || !dspd) return -1;
	end = mem_id + extent;
	/* Do we own the physical page number range? */
	/* FIXME: permission check */
//	if (end >= spd->pfn_base + mem_id) return -EINVAL;

	switch(op) {
	case COS_PFN_GRANT:
		/* Given "grant" access to the destination component for the pfn range */
		dspd->pfn_base   = mem_id;
		dspd->pfn_extent = extent;
		break;
	case COS_PFN_MAX_MEM:
		ret = spd->pfn_extent;
		break;
	default: return -1;
	}

	return ret;
}

/* 
 * The problem solved here is this: Each component has a page-table
 * that defines its memory mappings.  This is updated by the
 * mmap_cntl, and vas_cntl system calls.  However, there can be
 * multiple composite protection domains (due to MPD) that include
 * this component.  The question is how do they all stay consistent.
 * This is the function that maintains this.  On a page-fault, the
 * composite page-table is updated from the main component's
 * page-tables if the mapping is not present in the composite, but is
 * in the master.
 */
int 
fault_update_mpd_pgtbl(struct thread *thd, struct pt_regs *regs, vaddr_t fault_addr)
{
	struct spd *origin;
	struct spd_poly *active;

	origin = thd_get_thd_spd(thd);
	if (origin != virtual_namespace_query(fault_addr)) return 0;
	active = thd_get_thd_spdpoly(thd);

	if ( chal_pgtbl_entry_absent(origin->spd_info.pg_tbl, fault_addr)) return 0;
	if (!chal_pgtbl_entry_absent(active->pg_tbl, fault_addr)) return 0;

	chal_pgtbl_copy_range(active->pg_tbl, origin->spd_info.pg_tbl, fault_addr, HPAGE_SIZE);

	return 1;
}

#define MAX_LEN 512 /* keep consistent as in printc.h */
COS_SYSCALL int 
cos_syscall_print(int spdid, char *str, int len)
{
	char kern_buf[MAX_LEN];
	/*
	 * FIXME: use linux functions to copy the string into local
	 * storage to avoid faults.  ...This won't work with cos
	 * allocated memory, so we really just need to do a proper
	 * output system.  This is low prio as the string should be
	 * passed in the arg region.  Perhaps we should just check
	 * that.
	 */
	if (len < 1) return 0;
	if (len >= MAX_LEN) len = MAX_LEN - 1;
	memcpy(kern_buf, str, len);
	kern_buf[len] = '\0';
	printk("%s", kern_buf);

	return 0;
}

COS_SYSCALL long 
cos_syscall_cap_cntl(int spdid, int option, u32_t arg1, long arg2)
{
	vaddr_t va;
	u16_t capid;
	int ret = 0;
	struct spd *cspd, *sspd = NULL;
	spdid_t cspdid, sspdid;

	/* TODO: access control */

	cspdid = arg1 >> 16;
	cspd = spd_get_by_index(cspdid);
	if (!cspd) return -1;
	if (option == COS_CAP_GET_INVCNT) {
		sspdid = 0xFFFF & arg1;
		sspd = spd_get_by_index(sspdid);
		if (!sspd) return -1;
	} else {
		capid =  0xFFFF & arg1;
	}

	switch (option) {
	case COS_CAP_GET_INVCNT:
		va = chal_pgtbl_vaddr2kaddr(cspd->spd_info.pg_tbl, (unsigned long)cspd->user_vaddr_cap_tbl);
		assert((vaddr_t)cspd->user_cap_tbl == va);
		
		ret = spd_read_reset_invocation_cnt(cspd, sspd);
		break;
	case COS_CAP_SET_FAULT:
		if (spd_cap_set_fault_handler(cspd, capid, arg2)) ret = -1;
		break;
	case COS_CAP_SET_CSTUB:
		if (spd_cap_set_cstub(cspd, capid, arg2)) ret = -1;
		break;
	case COS_CAP_SET_SSTUB:
		if (spd_cap_set_sstub(cspd, capid, arg2)) ret = -1;
		break;
	case COS_CAP_SET_SERV_FN:
		if (spd_cap_set_sfn(cspd, capid, arg2)) ret = -1;
		break;
	case COS_CAP_ACTIVATE:
		/* arg2 == dest spd id */
		if (!spd_is_active(cspd) || 0 == arg2) {
			ret = -1;
			break;
		}
		sspd = spd_get_by_index((spdid_t)arg2);
		if (!sspd || spd_cap_set_dest(cspd, capid, sspd)) {
			ret = -1;
			break;
		}
		if (spd_cap_activate(cspd, capid)) ret = -1;
		break;
	default:
		ret = -1;
		break;
	};

	return ret;
}

COS_SYSCALL int 
cos_syscall_stats(int spdid)
{
	cos_meas_report();
	cos_meas_init();

	return 0;
}

extern int cos_syscall_idle(void);
COS_SYSCALL int 
cos_syscall_idle_cont(int spdid)
{
	struct thread *c = core_get_curr_thd();
	
	if (c != core_get_curr_thd()) return COS_SCHED_RET_AGAIN;

	chal_idle();

	return COS_SCHED_RET_SUCCESS;
}

COS_SYSCALL int 
cos_syscall_spd_cntl(int id, int op_spdid, long arg1, long arg2)
{
	struct spd *spd;
	short int op;
	spdid_t spd_id;
	int ret = 0;

	op = op_spdid >> 16;
	spd_id = op_spdid & 0xFFFF;
	
	if (COS_SPD_CREATE != op) {
		spd = spd_get_by_index(spd_id);
		if (!spd) return  -1;
	}

	switch (op) {
	case COS_SPD_CREATE:
	{
		paddr_t pa;

		pa = spd_alloc_pgtbl();
		if (0 == pa) {
			ret = -1;
			break;
		}
		spd = spd_alloc(0, NULL, 0);
		if (!spd) {
			spd_free_pgtbl(pa);
			ret = -1;
			break;
		}

		spd->spd_info.pg_tbl = pa;
		ret = (int)spd_get_index(spd);

		break;
	}
	case COS_SPD_DELETE:
		/* FIXME: check reference counts, mpds, etc... */

		spd_free_pgtbl(spd->spd_info.pg_tbl);
		spd->spd_info.pg_tbl = 0;
		if (spd->composite_spd != &spd->spd_info) {
			printk("FIXME: proper deletion of mpds not implemented.\n");
		}
		/* FIXME: check that capabilities have been
		 * dealloced, that refcnt is 0, etc... */
		spd_free(spd);
		break;
	case COS_SPD_RESERVE_CAPS:
		/* arg1 == number of caps */
		if (spd_reserve_cap_range(spd, (int)arg1) == -1) {
			ret = -1;
			break;
		}
		break;
	case COS_SPD_RELEASE_CAPS:
		if (spd_release_cap_range(spd) == -1) ret = -1;
		break;
	case COS_SPD_LOCATION:
		/* location already set */
		if (spd->location[0].size ||
		    spd_add_location(spd, arg1, arg2)) {
			ret = -1;
			break;
		}
		
		break;
	case COS_SPD_UCAP_TBL:
	{
		if (spd->user_vaddr_cap_tbl) {
			ret = -1;
			break;
		}
		/* arg1 = vaddr of ucap tbl */
		spd->user_vaddr_cap_tbl = (struct usr_inv_cap*)arg1;
		break;
	}
	case COS_SPD_ATOMIC_SECT:
		/* arg2 == atomic section index, arg1 == section address */
		if (arg2 >= COS_NUM_ATOMIC_SECTIONS || arg2 < 0) ret = -1;
		else spd->atomic_sections[arg2] = (vaddr_t)arg1;
		break;
	case COS_SPD_UPCALL_ADDR:
		/* arg1 = upcall_entry address */
		spd->upcall_entry = (vaddr_t)arg1;
		break;
	case COS_SPD_ACTIVATE:
	{
		struct composite_spd *cspd;
		vaddr_t kaddr;

		/* Have we set the virtual address space, caps, cap tbl*/
		if (!spd->user_vaddr_cap_tbl ||
		    !spd->spd_info.pg_tbl || !spd->location[0].lowest_addr ||
		    !spd->cap_base || !spd->cap_range) {
			printk("cos: spd_cntl -- cap tbl, location, or capability range not set (error %d).\n",
			       !spd->user_vaddr_cap_tbl      ? 1 : 
			       !spd->spd_info.pg_tbl         ? 2 :
			       !spd->location[0].lowest_addr ? 3 :
			       !spd->cap_base                ? 4 : 5);
			ret = -1;
			break;
		}
		if ((unsigned int)spd->user_vaddr_cap_tbl < spd->location[0].lowest_addr || 
		    (unsigned int)spd->user_vaddr_cap_tbl + sizeof(struct usr_inv_cap) * spd->cap_range > 
		    spd->location[0].lowest_addr + spd->location[0].size || 
		    !user_struct_fits_on_page((unsigned int)spd->user_vaddr_cap_tbl, sizeof(struct usr_inv_cap) * spd->cap_range)) {
			printk("cos: user capability table @ %x does not fit into spd, or onto a single page\n", 
			       (unsigned int)spd->user_vaddr_cap_tbl);
			ret = -1;
			break;
		}
		/* Is the ucap tbl mapped in? */
		kaddr = chal_pgtbl_vaddr2kaddr(spd->spd_info.pg_tbl, (vaddr_t)spd->user_vaddr_cap_tbl);
		if (0 == kaddr) {
			ret = -1;
			break;
		}
		spd->user_cap_tbl = (struct usr_inv_cap*)kaddr;
		spd_add_static_cap(spd, 0, spd, 0);

		cspd = spd_alloc_mpd();
		if (!cspd) {
			spd->user_cap_tbl = 0;
			ret = -1;
			break;
		}
		if (spd_composite_add_member(cspd, spd)) {
			spd_mpd_release(cspd);
			spd->user_cap_tbl = 0;
			printk("cos: could not add spd %d to composite spd %d.\n",
			       spd_get_index(spd), spd_mpd_index(cspd));
			ret = -1;
			break;
		}
		spd_make_active(spd);

		assert(spd->composite_spd);

		break;
	}
	default:
		ret = -1;
	}
	
	return ret;
}

COS_SYSCALL int 
cos_syscall_vas_cntl(int id, int op_spdid, long addr, long sz)
{
	int ret = 0;
	short int op;
	spdid_t spd_id;
	struct spd *spd;

	op = op_spdid >> 16;
	spd_id = op_spdid & 0xFFFF;
	spd = spd_get_by_index(spd_id);
	if (!spd) return -1;
	if (!sz)  return -1;

	switch(op) {
	case COS_VAS_CREATE: 	/* new vas */
	case COS_VAS_DELETE:	/* remove vas */
	case COS_VAS_SPD_ADD:	/* add spd to vas */
	case COS_VAS_SPD_REM:	/* remove spd from vas */
	default:
		printk("vas_cntl: undefined operation %d.\n", op);
		ret = -1;
		break;
	case COS_VAS_SPD_EXPAND:	/* allocate more vas to spd */
		if (spd_add_location(spd, addr, sz)) ret = -1;
		break;
	case COS_VAS_SPD_RETRACT:	/* deallocate some vas from spd */
		if (spd_rem_location(spd, addr, sz)) ret = -1;
		break;
	}

	return ret;
}

extern int send_ipi(int cpuid, int thdid, int wait);

//volatile int kern_tsc = 0;
/* The IPI calls should not be accessible from user level. This is for
 * test purpose only. Will remove this syscall. */
COS_SYSCALL int 
cos_syscall_send_ipi(int spd_id, long cpuid, int thdid, long arg)
{
	int wait, option;
	option = arg & 0xFFFF;
	wait = arg >> 16;
	assert(wait == 0);

	return send_ipi(cpuid, thdid, wait);
}

/* 
 * Composite's system call table that is indexed and invoked by ipc.S.
 * The user-level stubs are created in cos_component.h.
 */
void *cos_syscall_tbl[32] = {
	(void*)cos_syscall_void,
	(void*)cos_syscall_stats,
	(void*)cos_syscall_print,
	(void*)cos_syscall_create_thread,
	(void*)cos_syscall_switch_thread,
	(void*)cos_syscall_brand_wait,
	(void*)cos_syscall_brand_upcall,
	(void*)cos_syscall_brand_cntl,
	(void*)cos_syscall_upcall,
	(void*)cos_syscall_sched_cntl,
	(void*)cos_syscall_mpd_cntl,
	(void*)cos_syscall_mmap_cntl,
	(void*)cos_syscall_brand_wire,
	(void*)cos_syscall_cap_cntl,
	(void*)cos_syscall_buff_mgmt,
	(void*)cos_syscall_thd_cntl,
	(void*)cos_syscall_idle,
	(void*)cos_syscall_spd_cntl,
	(void*)cos_syscall_vas_cntl,
	(void*)cos_syscall_trans_cntl,
	(void*)cos_syscall_pfn_cntl,
	(void*)cos_syscall_send_ipi,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void
};
 
