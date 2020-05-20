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
#include "chal/call_convention.h"
#include "pgtbl.h"
#include "retype_tbl.h"
#include "tcap.h"
#include "list.h"

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long    sp, ip; /* to return to */
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
	int            isbound, pending, refcnt, is_init;
	sched_tok_t    sched_count;
	struct tcap *  rcvcap_tcap;      /* This rcvcap's tcap */
	struct thread *rcvcap_thd_notif; /* The parent rcvcap thread for notifications */
};

typedef enum {
	THD_STATE_PREEMPTED = 1,
	THD_STATE_RCVING    = 1 << 1, /* report to parent rcvcap that we're receiving */
} thd_state_t;

/**
 * The thread descriptor.  Contains all information pertaining to a
 * thread including its registers, id, rcvcap information, and, most
 * importantly, the kernel invocation stack of execution through
 * components.
 */
struct thread {
	thdid_t        tid;
	u16_t          invstk_top;
	struct pt_regs regs;
	struct pt_regs fault_regs;
	struct cos_fpu fpu;

	/* TODO: same cache-line as the tid */
	struct invstk_entry invstk[THD_INVSTK_MAXSZ];

	thd_state_t    state;
	u32_t          tls;
	cpuid_t        cpuid;
	unsigned int   refcnt;
	tcap_res_t     exec; /* execution time */
	tcap_time_t    timeout;
	struct thread *interrupted_thread;
	struct thread *scheduler_thread;
	struct cos_dcb_info *dcbinfo;

	/* rcv end-point data-structures */
	struct rcvcap_info rcvcap;
	struct list        event_head; /* all events for *this* end-point */
	struct list_node   event_list; /* the list of events for another end-point */
	u64_t              event_epoch; /* used by user-level for ULSCHED events.. */
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
	struct thread *   t;
	cpuid_t           cpuid;
} __attribute__((packed));

#include "dcb.h"

static void
thd_upcall_setup(struct thread *thd, u32_t entry_addr, int option, int arg1, int arg2, int arg3)
{
	struct pt_regs *r = &thd->regs;

	r->cx = option;

	r->bx = arg1;
	r->di = arg2;
	r->si = arg3;

	r->ip = r->dx = entry_addr;
	r->ax         = thd->tid | (get_cpuid() << 16); // thd id + cpu id

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
	return cos_faa((int *)&free_thd_id, 1);
}
static void
thd_rcvcap_take(struct thread *t)
{
	t->rcvcap.refcnt++;
}

static void
thd_rcvcap_release(struct thread *t)
{
	t->rcvcap.refcnt--;
}

static inline int
thd_rcvcap_isreferenced(struct thread *t)
{
	return t->rcvcap.refcnt > 0;
}

static inline int
thd_bound2rcvcap(struct thread *t)
{
	return t->rcvcap.isbound;
}

static inline int
thd_rcvcap_is_sched(struct thread *t)
{
	return thd_rcvcap_isreferenced(t);
}

static inline struct tcap *
thd_rcvcap_tcap(struct thread *t)
{
	return t->rcvcap.rcvcap_tcap;
}

static int
thd_rcvcap_isroot(struct thread *t)
{
	return t == t->rcvcap.rcvcap_thd_notif;
}

static inline struct thread *
thd_rcvcap_sched(struct thread *t)
{
	if (thd_rcvcap_isreferenced(t)) return t;
	return t->rcvcap.rcvcap_thd_notif;
}

static void
thd_next_thdinfo_update(struct cos_cpu_local_info *cli, struct thread *thd, struct tcap *tc, tcap_prio_t prio,
                        tcap_res_t budget)
{
	struct next_thdinfo *nti = &cli->next_ti;

	nti->thd    = thd;
	nti->tc     = tc;
	nti->prio   = prio;
	nti->budget = budget;
}

static void
thd_rcvcap_init(struct thread *t, int is_init)
{
	struct rcvcap_info *rc = &t->rcvcap;

	rc->isbound = rc->pending = rc->refcnt = 0;
	rc->sched_count                        = 0;
	rc->is_init                            = is_init;
	rc->rcvcap_thd_notif                   = NULL;
}

static inline struct comp_info *
thd_invstk_peek_compinfo(struct thread *curr_thd, struct cos_cpu_local_info *cos_info, int peek_index)
{
	/* curr_thd should be the current thread! We are using cached invstk_top. */
	return &(curr_thd->invstk[peek_index].comp_info);
}

static inline int
thd_rcvcap_evt_pending(struct thread *t)
{
	return !list_isempty(&t->event_head);
}

static inline void
thd_rcvcap_evt_enqueue(struct thread *head, struct thread *t)
{
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	struct comp_info *c = thd_invstk_peek_compinfo(head, cos_info, 0); /* in its root component! */
	struct cos_scb_info   *scb = NULL;
	struct cos_sched_ring *r   = NULL;

	if (list_empty(&t->event_list) && head != t) list_enqueue(&head->event_head, &t->event_list);
	if (unlikely(!c ||!c->scb_data)) return;

	scb = ((c->scb_data) + get_cpuid());
	r   = &(scb->sched_events);
	r->more = thd_rcvcap_evt_pending(head);
}

static inline void
thd_list_rem(struct thread *head, struct thread *t)
{
	list_rem(&t->event_list);
}

static inline struct thread *
thd_rcvcap_evt_dequeue(struct thread *head)
{
	return list_dequeue(&head->event_head);
}

/*
 * If events are going to be delivered on this thread, then we should
 * be tracking its execution time.  Thus, we co-opt the event list as
 * a notification trigger for tracking the thread's execution time.
 */
static inline int
thd_track_exec(struct thread *t)
{
	return !list_empty(&t->event_list);
}

static inline int
thd_rcvcap_pending(struct thread *t)
{
	if (t->rcvcap.pending || (t->dcbinfo && t->dcbinfo->pending)) return 1;
	return thd_rcvcap_evt_pending(t);
}

static inline sched_tok_t
thd_rcvcap_get_counter(struct thread *t)
{
	return t->rcvcap.sched_count;
}

static inline void
thd_rcvcap_set_counter(struct thread *t, sched_tok_t cntr)
{
	t->rcvcap.sched_count = cntr;
}

static inline void
thd_rcvcap_pending_set(struct thread *arcvt)
{
	if (likely(arcvt->dcbinfo)) arcvt->dcbinfo->pending = 1;
	else arcvt->rcvcap.pending = 1;
}

static inline void
thd_rcvcap_pending_reset(struct thread *arcvt)
{
	if (likely(arcvt->dcbinfo)) arcvt->dcbinfo->pending = 0;
	else arcvt->rcvcap.pending = 0;
}

static inline int
thd_state_evt_deliver(struct thread *t, unsigned long *thd_state, unsigned long *cycles, unsigned long *timeout, u64_t *epoch)
{
	struct thread *e = thd_rcvcap_evt_dequeue(t);

	assert(thd_bound2rcvcap(t));
	if (!e) return 0;

	*thd_state = e->tid | (e->state & THD_STATE_RCVING ? (thd_rcvcap_pending(e) ? 0 : 1 << 31) : 0);
	*cycles    = e->exec;
	e->exec    = 0;
	*timeout   = e->timeout;
	e->timeout = 0;
	*epoch     = e->event_epoch;
	e->event_epoch = 0;

	return 1;
}

static inline struct thread *
thd_current(struct cos_cpu_local_info *cos_info)
{
	return (struct thread *)(cos_info->curr_thd);
}

static inline void
thd_current_update(struct thread *next, struct thread *prev, struct cos_cpu_local_info *cos_info)
{
	/* commit the cached data */
	prev->invstk_top = cos_info->invstk_top;
	cos_info->invstk_top = next->invstk_top;
	cos_info->curr_thd   = next;
}

static inline struct thread *
thd_scheduler(struct thread *thd)
{
	return thd->scheduler_thread;
}

static inline void
thd_scheduler_set(struct thread *thd, struct thread *sched)
{
	if (unlikely(thd->scheduler_thread != sched)) thd->scheduler_thread = sched;
}

static inline int
thd_activate(struct captbl *t, capid_t cap, capid_t capin, struct thread *thd, capid_t compcap, thdclosure_index_t init_data, capid_t dcbcap, unsigned short dcboff)
{
	struct cos_cpu_local_info *cli = cos_cpu_local_info();
	struct cap_thd            *tc = NULL;
	struct cap_comp           *compc = NULL;
	struct cap_dcb            *dc = NULL;
	int                        ret;

	memset(thd, 0, sizeof(struct thread));
	compc = (struct cap_comp *)captbl_lkup(t, compcap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;
	if (likely(dcbcap)) {
		dc    = (struct cap_dcb *)captbl_lkup(t, dcbcap);
		if (unlikely(!dc || dc->h.type != CAP_DCB)) return -EINVAL;
		if (dcboff > PAGE_SIZE / sizeof(struct cos_dcb_info)) return -EINVAL;
	}

	tc = (struct cap_thd *)__cap_capactivate_pre(t, cap, capin, CAP_THD, &ret);
	if (!tc) return ret;

	/* initialize the thread */
	memcpy(&(thd->invstk[0].comp_info), &compc->info, sizeof(struct comp_info));
	thd->invstk[0].ip = thd->invstk[0].sp = 0;
	thd->tid                              = thdid_alloc();
	thd->refcnt                           = 1;
	thd->invstk_top                       = 0;
	thd->cpuid                            = get_cpuid();
	if (likely(dc)) {
		ret = dcb_thd_ref(dc, thd);
		if (ret) goto err; /* TODO: cleanup captbl slot */
		thd->dcbinfo = (struct cos_dcb_info *)(dc->kern_addr + (dcboff * sizeof(struct cos_dcb_info)));
		memset(thd->dcbinfo, 0, sizeof(struct cos_dcb_info));
	}
	assert(thd->tid <= MAX_NUM_THREADS);
	thd_scheduler_set(thd, thd_current(cli));

	/* TODO: fix the way to specify scheduler in a component! */
	thd_rcvcap_init(thd, !init_data);
	list_head_init(&thd->event_head);
	list_init(&thd->event_list, thd);

	thd_upcall_setup(thd, compc->entry_addr, COS_UPCALL_THD_CREATE, init_data, 0, 0);

	/* initialize the capability */
	tc->t     = thd;
	tc->cpuid = get_cpuid();
	__cap_capactivate_post(&tc->h, CAP_THD);

	return 0;

err:
	return ret;
}

static inline int
thd_migrate_cap(struct captbl *ct, capid_t thd_cap)
{
	struct thread *thd;
	struct cap_thd *tc;

	/* we migrated the capability to core */
	tc = (struct cap_thd *)captbl_lkup(ct, thd_cap);
	if (!tc || tc->h.type != CAP_THD || get_cpuid() != tc->cpuid) return -EINVAL;
	thd = tc->t;
	tc->cpuid = thd->cpuid;

	return 0;
}

static inline int
thd_migrate(struct captbl *ct, capid_t thd_cap, cpuid_t core)
{
	struct thread *thd;
	struct cap_thd *tc;

	tc = (struct cap_thd *)captbl_lkup(ct, thd_cap);
	if (!tc || tc->h.type != CAP_THD || get_cpuid() != tc->cpuid) return -EINVAL;
	thd = tc->t;
	if (NUM_CPU < 2 || core >= NUM_CPU || core < 0) return -EINVAL;
	if (tc->cpuid != thd->cpuid) return -EINVAL; /* outdated capability */
	if (thd->cpuid == core) return -EINVAL; /* already migrated. invalid req */
	if (thd->cpuid != get_cpuid()) return -EPERM; /* only push migration */

	if (thd_current(cos_cpu_local_info()) == thd) return -EPERM; /* not a running thread! */
	if (thd->invstk_top > 0) return -EPERM;  /* not if its in an invocation */
	if (thd_bound2rcvcap(thd) || thd->rcvcap.rcvcap_thd_notif) return -EPERM; /* not if it's an AEP */
	if (thd->rcvcap.rcvcap_tcap) return -EPERM; /* not if it has its own tcap on this core */

	thd->scheduler_thread = NULL;
	thd->cpuid = core;
	/* we also migrated the capability to core */
	tc->cpuid = core;

	/* 
	 * TODO:
	 * given that the thread is not running right now, 
	 * and we don't allow migrating a thread that's in an invocation for now,
	 * i think we can find the COREID_OFFSET/CPUID_OFFSET on stack and fix the
	 * core id right here?? 
	 */

	return 0;
}

static inline int
thd_deactivate(struct captbl *ct, struct cap_captbl *dest_ct, unsigned long capin, livenessid_t lid, capid_t pgtbl_cap,
               capid_t cosframe_addr, capid_t dcbcap, const int root)
{
	struct cos_cpu_local_info *cli = cos_cpu_local_info();
	struct cap_header         *thd_header;
	struct thread             *thd;
	struct cap_dcb            *dcb = NULL;
	unsigned long              old_v = 0, *pte = NULL;
	int                        ret;

	thd_header = captbl_lkup(dest_ct->captbl, capin);
	if (!thd_header || thd_header->type != CAP_THD) cos_throw(err, -EINVAL);
	thd = ((struct cap_thd *)thd_header)->t;
	assert(thd->refcnt);
	if (dcbcap) {
		dcb = (struct cap_dcb *)captbl_lkup(ct, dcbcap);
		if (!dcb || dcb->h.type != CAP_DCB) cos_throw(err, -EINVAL);
	}

	if (thd->refcnt == 1) {
		if (!root) cos_throw(err, -EINVAL);
		/* last ref cannot be removed if bound to arcv cap */
		if (thd_bound2rcvcap(thd)) cos_throw(err, -EBUSY);
		/*
		 * Last reference. Require pgtbl and cos_frame cap to
		 * release the kmem page.
		 */
		ret = kmem_deact_pre(thd_header, ct, pgtbl_cap, cosframe_addr, &pte, &old_v);
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

	if (dcb) {
		ret = dcb_thd_deref(dcb, thd);
		if (ret) cos_throw(err, ret);
	}
	ret = cap_capdeactivate(dest_ct, capin, CAP_THD, lid);
	if (ret) cos_throw(err, ret);

	thd->refcnt--;
	/* deactivation success */
	if (thd->refcnt == 0) {
		if (cli->next_ti.thd == thd) thd_next_thdinfo_update(cli, 0, 0, 0, 0);

		/* move the kmem for the thread to a location
		 * in a pagetable as COSFRAME */
		ret = kmem_deact_post(pte, old_v);
		if (ret) cos_throw(err, ret);
	}

	return 0;
err:
	return ret;
}

static inline int
thd_tls_set(struct captbl *ct, capid_t thd_cap, vaddr_t tlsaddr, struct thread *current)
{
	struct cap_thd *tc;
	struct thread * thd;

	tc = (struct cap_thd *)captbl_lkup(ct, thd_cap);
	if (!tc || tc->h.type != CAP_THD || get_cpuid() != tc->cpuid) return -EINVAL;

	thd = tc->t;
	assert(thd);
	thd->tls = tlsaddr;

	if (current == thd) chal_tls_update(tlsaddr);

	return 0;
}

static inline void
thd_init(void)
{
	assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD));
	// assert(offsetof(struct thread, regs) == 4); /* see THD_REGS in entry.S */
}

static inline int
curr_invstk_inc(struct cos_cpu_local_info *cos_info)
{
	return cos_info->invstk_top++;
}

static inline int
curr_invstk_dec(struct cos_cpu_local_info *cos_info)
{
	return cos_info->invstk_top--;
}

static inline int
curr_invstk_top(struct cos_cpu_local_info *cos_info)
{
	return cos_info->invstk_top;
}

static inline struct comp_info *
thd_invstk_current_compinfo(struct thread *curr_thd, struct cos_cpu_local_info *cos_info)
{
	return &(curr_thd->invstk[curr_invstk_top(cos_info)].comp_info);
}

static inline struct comp_info *
thd_invstk_current(struct thread *curr_thd, unsigned long *ip, unsigned long *sp, struct cos_cpu_local_info *cos_info)
{
	/* curr_thd should be the current thread! We are using cached invstk_top. */
	struct invstk_entry *curr;

	curr = &curr_thd->invstk[curr_invstk_top(cos_info)];
	*ip  = curr->ip;
	*sp  = curr->sp;

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
thd_invstk_push(struct thread *thd, struct comp_info *ci, unsigned long ip, unsigned long sp,
                struct cos_cpu_local_info *cos_info)
{
	struct invstk_entry *top, *prev;

	if (unlikely(curr_invstk_top(cos_info) >= THD_INVSTK_MAXSZ)) return -1;

	prev = &thd->invstk[curr_invstk_top(cos_info)];
	top  = &thd->invstk[curr_invstk_top(cos_info) + 1];
	curr_invstk_inc(cos_info);
	prev->ip = ip;
	prev->sp = sp;
	memcpy(&top->comp_info, ci, sizeof(struct comp_info));
	top->ip = top->sp = 0;

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
	curr->state |= THD_STATE_PREEMPTED;
	next->interrupted_thread = curr;
	memcpy(&curr->regs, regs, sizeof(struct pt_regs));
}

static inline int
thd_sched_events_produce(struct thread *thd, struct cos_cpu_local_info *cos_info)
{
	int delta = 0, inv_top = curr_invstk_top(cos_info);
	struct cos_scb_info   *scb = NULL;
	struct cos_sched_ring *r   = NULL;
	struct comp_info      *c   = NULL;

	if (unlikely(inv_top != 0 || thd->rcvcap.is_init == 0)) return 0;

	c = thd_invstk_peek_compinfo(thd, cos_info, inv_top);
	if (unlikely(!c || !c->scb_data)) return -ENOENT;

	scb = ((c->scb_data) + get_cpuid());
	r   = &(scb->sched_events);
	/* 
	 * only produce more if the ring is empty! 
	 * so the user only calls after dequeueing all previous events. 
	 */
	if (unlikely(r->head != r->tail)) return -EAGAIN;

	r->head = r->tail = 0;
	while (delta < COS_SCHED_EVENT_RING_SIZE) {
		struct cos_sched_event *e = &(r->event_buf[delta]);
		unsigned long thd_state;

		if (!thd_state_evt_deliver(thd, &thd_state, (unsigned long *)&(e->evt.elapsed_cycs),
					(unsigned long *)&(e->evt.next_timeout), &(e->evt.epoch))) break;
		e->tid         = (thd_state << 1) >> 1;
		e->evt.blocked = (thd_state >> 31);

		delta++;
	}

	r->tail += delta;
	r->more  = thd_rcvcap_evt_pending(thd);

	return delta;
}

static inline void
thd_rcvcap_pending_deliver(struct thread *thd, struct pt_regs *regs)
{
	unsigned long thd_state = 0, cycles = 0, timeout = 0;
	u64_t epoch = 0;

	/* events only in scb now, no return values... */
	thd_rcvcap_pending_reset(thd);
	if (thd_sched_events_produce(thd, cos_cpu_local_info()) == -ENOENT) {
		thd_state_evt_deliver(thd, &thd_state, &cycles, &timeout, &epoch);
	}
	__userregs_setretvals(regs, thd_rcvcap_pending(thd), thd_state, cycles, timeout);
}

static inline int
thd_switch_update(struct thread *thd, struct pt_regs *regs, int issame)
{
	int preempt = 0, pending = 0;

	/* TODO: check FPU */
	/* fpu_save(thd); */
	if (thd->state & THD_STATE_PREEMPTED) {
		/* TODO: assert that its a scheduler thread */
		/* assert(!(thd->state & THD_STATE_RCVING)); */
		thd->state &= ~THD_STATE_PREEMPTED;
		preempt = 1;
	}

	/* FIXME: can the thread be in race with the kernel? */
	if (thd->state & THD_STATE_RCVING) {
		assert(!(thd->state & THD_STATE_PREEMPTED));
		thd->state &= ~THD_STATE_RCVING;
		thd_rcvcap_pending_deliver(thd, regs);
		pending = thd_rcvcap_pending(thd);
		/*
		 * If a scheduler thread was running using child tcap and blocked on RCVING
		 * and budget expended logic decided to run the scheduler thread with it's
		 * tcap, then curr_thd == next_thd and state will be RCVING.
		 */
	}

	if (unlikely(thd->dcbinfo && thd->dcbinfo->sp)) {
		assert(preempt == 0);
		regs->dx = regs->ip = thd->dcbinfo->ip + DCB_IP_KERN_OFF;
		regs->cx = regs->sp = thd->dcbinfo->sp;
		thd->dcbinfo->sp = 0;
	}

	if (issame && preempt == 0) {
		__userregs_set(regs, pending, __userregs_getsp(regs), __userregs_getip(regs));
	}

	return preempt;
}

static inline int
thd_introspect(struct thread *t, unsigned long op, unsigned long *retval)
{
	switch (op) {
	case THD_GET_TID:
		*retval = t->tid;
		break;
	case THD_GET_DCB_IP:
		*retval = t->dcbinfo->ip;
		break;
	case THD_GET_DCB_SP:
		*retval = t->dcbinfo->sp;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#endif /* THD_H */
