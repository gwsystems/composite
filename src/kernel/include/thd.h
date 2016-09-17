/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef THD_H
#define THD_H

#include "component.h"
#include "cap_ops.h"
#include "fpu_regs.h"
#include "chal/cpuid.h"
#include "pgtbl.h"
#include "retype_tbl.h"
#include "tcap.h"
#include "list.h"

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
} HALF_CACHE_ALIGNED;

#define THD_INVSTK_MAXSZ 32

/*
 * This is the data structure embedded in threads that are associated
 * with an asynchronous receive end-point.  It tracks the reference
 * count on that rcvcap, the tcap associated with it, and the thread
 * associated with the rcvcap that receives notifications for this
 * rcvcap.
 */
struct rcvcap_info {
	/* how many other arcv end-points send notifications to this one? */
	int isbound, pending, refcnt;
	sched_tok_t sched_count;
	struct tcap   *rcvcap_tcap;      /* This rcvcap's tcap */
	struct thread *rcvcap_thd_notif; /* The parent rcvcap thread for notifications */
};

typedef enum {
	THD_STATE_PREEMPTED   = 1,
	THD_STATE_RCVING      = 1<<1, /* report to parent rcvcap that we're receiving */
} thd_state_t;

/**
 * The thread descriptor.  Contains all information pertaining to a
 * thread including its registers, id, rcvcap information, and, most
 * importantly, the kernel invocation stack of execution through
 * components.
 */
struct thread {
	thdid_t tid;
	u16_t invstk_top;
        struct pt_regs regs;
        struct pt_regs fault_regs;
        struct cos_fpu fpu;

	/* TODO: same cache-line as the tid */
	struct invstk_entry invstk[THD_INVSTK_MAXSZ];

	thd_state_t  state;
	u32_t        tls;
	cpuid_t      cpuid;
	unsigned int refcnt;
	tcap_res_t   exec;   /* execution time */
	unsigned int sw_counter; 	/* switch counter for user-level race-cond check */
	struct thread *interrupted_thread;

	/* rcv end-point data-structures */
	struct rcvcap_info rcvcap;
	struct list        event_head; /* all events for *this* end-point */
	struct list_node   event_list; /* the list of events for another end-point */
} CACHE_ALIGNED;

/*
 * Thread capability descriptor that is minimal and contains only
 * consistency checking information (cpuid to ensure we're accessing
 * the thread on the correct core), and a pointer to the thread itself
 * that needs no synchronization (it is core-local, and interrupts are
 * disabled in the kernel).
 */
struct cap_thd {
	struct cap_header h;
	struct thread *t;
	cpuid_t cpuid;
} __attribute__((packed));

static void
thd_upcall_setup(struct thread *thd, u32_t entry_addr, int option, int arg1, int arg2, int arg3)
{
	struct pt_regs *r = &thd->regs;

	r->cx = option;

	r->bx = arg1;
	r->di = arg2;
	r->si = arg3;

	r->ip = r->dx = entry_addr;
	r->ax = thd->tid | (get_cpuid() << 16); // thd id + cpu id

	return;
}

/*
 * FIXME: We need global thread name space as we use thd_id to access
 * simple stacks. When we have low-level per comp stack free-list, we
 * don't have to use global thread id name space.
 *
 * Update: this is only partially true.  We should really just get rid
 * of this id in the kernel and replace it with a
 * scheduler-configurable variable.  That variable can be the thread
 * id where appropriate, and some other (component-controlled)
 * principal id otherwise.  Given this, the allocator should be in the
 * scheduler, not here.
 */
extern u32_t free_thd_id;
static u32_t
thdid_alloc(void)
{
        /* FIXME: thd id address space management. */
	if (unlikely(free_thd_id >= MAX_NUM_THREADS)) assert(0);
	return cos_faa((int*)&free_thd_id, 1);
}
static void
thd_rcvcap_take(struct thread *t)         { t->rcvcap.refcnt++; }

static void
thd_rcvcap_release(struct thread *t)      { t->rcvcap.refcnt--; }

static inline int
thd_rcvcap_isreferenced(struct thread *t) { return t->rcvcap.refcnt > 0; }

static inline int
thd_bound2rcvcap(struct thread *t)        { return t->rcvcap.isbound; }

static inline int
thd_rcvcap_is_sched(struct thread *t)     { return thd_rcvcap_isreferenced(t); }

static inline struct tcap *
thd_rcvcap_tcap(struct thread *t)         { return t->rcvcap.rcvcap_tcap; }

static int
thd_rcvcap_isroot(struct thread *t)       { return t == t->rcvcap.rcvcap_thd_notif; }

static inline struct thread *
thd_rcvcap_sched(struct thread *t)
{
	if (thd_rcvcap_isreferenced(t)) return t;
	return t->rcvcap.rcvcap_thd_notif;
}

static void
thd_rcvcap_init(struct thread *t)
{
	struct rcvcap_info *rc = &t->rcvcap;

	rc->isbound = rc->pending = rc->refcnt = 0;
	rc->sched_count = 0;
	rc->rcvcap_thd_notif = NULL;
}

static inline void
thd_rcvcap_evt_enqueue(struct thread *head, struct thread *t)
{ if (list_empty(&t->event_list) && head != t) list_enqueue(&head->event_head, &t->event_list); }

static inline void
thd_list_rem(struct thread *head, struct thread *t) { list_rem(&t->event_list); }

static inline struct thread *
thd_rcvcap_evt_dequeue(struct thread *head) { return list_dequeue(&head->event_head); }

/*
 * If events are going to be delivered on this thread, then we should
 * be tracking its execution time.  Thus, we co-opt the event list as
 * a notification trigger for tracking the thread's execution time.
 */
static inline int
thd_track_exec(struct thread *t) { return !list_empty(&t->event_list); }

static inline int
thd_state_evt_deliver(struct thread *t, unsigned long *thd_state, unsigned long *cycles)
{
	struct thread *e = thd_rcvcap_evt_dequeue(t);

	assert(thd_bound2rcvcap(t));
	if (!e) return 0;

	*thd_state = e->tid | (e->state & THD_STATE_RCVING ? 1<<31 : 0);
	*cycles    = e->exec;
	e->exec    = 0;

	return 1;
}

static int
thd_rcvcap_pending(struct thread *t)
{ return t->rcvcap.pending || list_first(&t->event_head) != NULL; }

static sched_tok_t
thd_rcvcap_get_counter(struct thread *t)
{ return t->rcvcap.sched_count; }

static void
thd_rcvcap_set_counter(struct thread *t, sched_tok_t cntr)
{ t->rcvcap.sched_count = cntr; }

static void
thd_rcvcap_pending_inc(struct thread *arcvt)
{ arcvt->rcvcap.pending++; }

static int
thd_rcvcap_pending_dec(struct thread *arcvt)
{
	int pending = arcvt->rcvcap.pending;

	if (pending == 0) return 0;
	arcvt->rcvcap.pending--;

	return pending;
}

static int
thd_activate(struct captbl *t, capid_t cap, capid_t capin, struct thread *thd, capid_t compcap, int init_data)
{
	struct cap_thd *tc;
	struct cap_comp *compc;
	int ret;

	memset(thd, 0, sizeof(struct thread));
	compc = (struct cap_comp *)captbl_lkup(t, compcap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	tc = (struct cap_thd *)__cap_capactivate_pre(t, cap, capin, CAP_THD, &ret);
	if (!tc) return ret;

	/* initialize the thread */
	memcpy(&(thd->invstk[0].comp_info), &compc->info, sizeof(struct comp_info));
	thd->invstk[0].ip = thd->invstk[0].sp = 0;
	thd->tid          = thdid_alloc();
	thd->refcnt       = 1;
     	thd->invstk_top   = 0;
	thd->cpuid        = get_cpuid();
	assert(thd->tid <= MAX_NUM_THREADS);

	thd_rcvcap_init(thd);
	list_head_init(&thd->event_head);
	list_init(&thd->event_list, thd);

	thd_upcall_setup(thd, compc->entry_addr, COS_UPCALL_THD_CREATE, init_data, 0, 0);

	/* initialize the capability */
	tc->t     = thd;
	tc->cpuid = get_cpuid();
	__cap_capactivate_post(&tc->h, CAP_THD);

	return 0;
}

static int
thd_deactivate(struct captbl *ct, struct cap_captbl *dest_ct, unsigned long capin,
	       livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr, const int root)
{
	struct cap_header *thd_header;
	struct thread *thd;
	unsigned long old_v = 0, *pte = NULL;
	int ret;

	thd_header = captbl_lkup(dest_ct->captbl, capin);
	if (!thd_header || thd_header->type != CAP_THD) cos_throw(err, -EINVAL);
	thd = ((struct cap_thd *)thd_header)->t;
	assert(thd->refcnt);

	if (thd->refcnt == 1) {
		if (!root) cos_throw(err, -EINVAL);
		/* last ref cannot be removed if bound to arcv cap */
		if (thd_bound2rcvcap(thd)) cos_throw(err, -EBUSY);
		/*
		 * Last reference. Require pgtbl and cos_frame cap to
		 * release the kmem page.
		 */
		ret = kmem_deact_pre(thd_header, ct, pgtbl_cap,
				     cosframe_addr, &pte, &old_v);
		if (ret) cos_throw(err, ret);
	} else {
		/* more reference exists. */
		if (root) cos_throw(err, -EINVAL);
		assert(thd->refcnt > 1);
		if (pgtbl_cap || cosframe_addr) {
			/* we pass in the pgtbl cap and frame addr,
			 * but ref_cnt is > 1. We'll ignore the two
			 * parameters as we won't be able to release
			 * the memory. */
			printk("cos: deactivating thread but not able to release kmem page (%p) yet (ref_cnt %d).\n",
			       (void *)cosframe_addr, thd->refcnt);
		}
	}

	ret = cap_capdeactivate(dest_ct, capin, CAP_THD, lid);
	if (ret) cos_throw(err, ret);

	thd->refcnt--;
	/* deactivation success */
	if (thd->refcnt == 0) {
		/* move the kmem for the thread to a location
		 * in a pagetable as COSFRAME */
		ret = kmem_deact_post(pte, old_v);
		if (ret) cos_throw(err, ret);
	}

	return 0;
err:
	return ret;
}

static int
thd_tls_set(struct captbl *ct, capid_t thd_cap, vaddr_t tlsaddr, struct thread *current)
{
	struct cap_thd *tc;
	struct thread *thd;

	tc = (struct cap_thd *)captbl_lkup(ct, thd_cap);
	if (!tc || tc->h.type != CAP_THD || get_cpuid() != tc->cpuid) return -EINVAL;

	thd = tc->t;
	assert(thd);
	thd->tls = tlsaddr;

	if (current == thd) chal_tls_update(tlsaddr);

	return 0;
}

static void
thd_init(void)
{
	assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD));
	//assert(offsetof(struct thread, regs) == 4); /* see THD_REGS in entry.S */
}

static inline struct thread *
thd_current(struct cos_cpu_local_info *cos_info)
{ return (struct thread *)(cos_info->curr_thd); }

static inline void
thd_current_update(struct thread *next, struct thread *prev, struct cos_cpu_local_info *cos_info)
{
	/* commit the cached data */
	prev->invstk_top     = cos_info->invstk_top;
	cos_info->invstk_top = next->invstk_top;
	cos_info->curr_thd   = next;
}

static inline int curr_invstk_inc(struct cos_cpu_local_info *cos_info)
{
	return cos_info->invstk_top++;
}

static inline int curr_invstk_dec(struct cos_cpu_local_info *cos_info)
{
	return cos_info->invstk_top--;
}

static inline int curr_invstk_top(struct cos_cpu_local_info *cos_info)
{
	return cos_info->invstk_top;
}

static inline struct comp_info *
thd_invstk_current(struct thread *curr_thd, unsigned long *ip, unsigned long *sp, struct cos_cpu_local_info *cos_info)
{
	/* curr_thd should be the current thread! We are using cached invstk_top. */
	struct invstk_entry *curr;

	curr = &curr_thd->invstk[curr_invstk_top(cos_info)];
	*ip = curr->ip;
	*sp = curr->sp;

	return &curr->comp_info;
}

static inline pgtbl_t
thd_current_pgtbl(struct thread *thd)
{
	struct invstk_entry *curr_entry;

	/* don't use the cached invstk_top here. We need the stack
	 * pointer of the specified thread. */
	curr_entry = &thd->invstk[thd->invstk_top];
	return curr_entry->comp_info.pgtbl;
}

static inline int
thd_invstk_push(struct thread *thd, struct comp_info *ci, unsigned long ip, unsigned long sp, struct cos_cpu_local_info *cos_info)
{
	struct invstk_entry *top, *prev;

	if (unlikely(curr_invstk_top(cos_info) >= THD_INVSTK_MAXSZ)) return -1;

	prev = &thd->invstk[curr_invstk_top(cos_info)];
	top  = &thd->invstk[curr_invstk_top(cos_info)+1];
	curr_invstk_inc(cos_info);
	prev->ip = ip;
	prev->sp = sp;
	memcpy(&top->comp_info, ci, sizeof(struct comp_info));
	top->ip  = top->sp = 0;

	return 0;
}

static inline struct comp_info *
thd_invstk_pop(struct thread *thd, unsigned long *ip, unsigned long *sp, struct cos_cpu_local_info *cos_info)
{
	if (unlikely(curr_invstk_top(cos_info) == 0)) return NULL;
	curr_invstk_dec(cos_info);
	return thd_invstk_current(thd, ip, sp, cos_info);
}

static inline void
thd_preemption_state_update(struct thread *curr, struct thread *next, struct pt_regs *regs)
{
	curr->state             |= THD_STATE_PREEMPTED;
	next->interrupted_thread = curr;
	memcpy(&curr->regs, regs, sizeof(struct pt_regs));
}

static inline int
thd_introspect(struct thread *t, unsigned long op, unsigned long *retval)
{
	switch(op) {
	case THD_GET_IP : *retval = t->regs.ip; break;
	case THD_GET_SP : *retval = t->regs.sp; break;
	case THD_GET_BP : *retval = t->regs.bp; break;
	case THD_GET_AX : *retval = t->regs.ax; break;
	case THD_GET_BX : *retval = t->regs.bx; break;
	case THD_GET_CX : *retval = t->regs.cx; break;
	case THD_GET_DX : *retval = t->regs.dx; break;
	case THD_GET_SI : *retval = t->regs.si; break;
	case THD_GET_DI : *retval = t->regs.di; break;
	case THD_GET_TID: *retval = t->tid; break;
	default: return -EINVAL;
	}
	return 0;
}

#endif /* THD_H */
