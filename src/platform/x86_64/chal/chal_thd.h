#ifndef CHAL_INVSTK
#define CHAL_INVSTK

/* code for hardware-specific thread routines */

#include "component.h"
#include "fpu_regs.h"


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
	int            isbound, pending, refcnt, is_all_pending;
	sched_tok_t    sched_count;
	struct tcap *  rcvcap_tcap;      /* This rcvcap's tcap */
	struct thread *rcvcap_thd_notif; /* The parent rcvcap thread for notifications */
};

typedef enum {
	THD_STATE_PREEMPTED = 1,
	THD_STATE_RCVING    = 1 << 1, /* report to parent rcvcap that we're receiving */
} thd_state_t;

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long    sp, ip;
	u8_t             ulinvstk_off;
} HALF_CACHE_ALIGNED;

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
	struct invstk_entry  invstk[THD_INVSTK_MAXSZ];

	struct cos_ulinvstk *ulinvstk;
	u32_t                ulinvstk_ktop; /* need to track if there are new ulinvstk entries */

	thd_state_t    state;
	u32_t          tls;
	cpuid_t        cpuid;
	unsigned int   refcnt;
	tcap_res_t     exec; /* execution time */
	tcap_time_t    timeout;
	struct thread *interrupted_thread;
	struct thread *scheduler_thread;

	/* rcv end-point data-structures */
	struct rcvcap_info rcvcap;
	struct list        event_head; /* all events for *this* end-point */
	struct list_node   event_list; /* the list of events for another end-point */
} CACHE_ALIGNED;

/* TODO: this is all MPK dependent. Make a version for no UL-invs */

/* define here instead of inv.h. Dependency shenanigans */

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


static inline pgtbl_t
chal_current_pgtbl(struct thread *thd)
{
	struct invstk_entry *curr_entry;

	/* don't use the cached invstk_top here. We need the stack
	 * pointer of the specified thread. */
	curr_entry = &thd->invstk[thd->invstk_top];
	return curr_entry->comp_info.pgtblinfo.pgtbl;
}

static inline struct comp_info *
chal_ulinvstk_current(struct cos_ulinvstk *stk, struct comp_info *origin)
{
	struct cos_ulinvstk_entry *ent = &stk->s[0];
	struct comp_info          *ci = origin;
	struct cap_sinv           *sinvcap;
	unsigned int               stk_iter;

	/* upper-bounded by ulinvstk sz */
	for (stk_iter = 0; stk_iter < COS_ULK_INVSTK_SZ && stk_iter < stk->top; stk_iter++) {
		ent = &stk->s[stk_iter];
		sinvcap = (struct cap_sinv *)captbl_lkup(ci->captbl, ent->sinv_cap);
		assert(sinvcap->h.type == CAP_SINV);
		ci = &sinvcap->comp_info;
	}

	return ci;
}

static inline struct comp_info *
chal_invstk_current(struct thread *curr_thd, unsigned long *ip, unsigned long *sp, struct cos_cpu_local_info *cos_info)
{
	/* curr_thd should be the current thread! We are using cached invstk_top. */
	struct invstk_entry *curr;
	struct cos_ulinvstk *ulinvstk;
	struct comp_info    *ci;

	curr = &curr_thd->invstk[curr_invstk_top(cos_info)];
	ulinvstk = curr_thd->ulinvstk;

	/* this thread makes no UL-invs */
	if (unlikely(!ulinvstk)) {
		*ip = curr->ip;
		*sp = curr->sp;
		return &curr->comp_info;
	}

	/* if there are new entries on the UL stack, 
	   we must be coming from a comp on the UL stack */
	if (ulinvstk->top > curr->ulinvstk_off) {
		ci = chal_ulinvstk_current(ulinvstk, &curr->comp_info);
	}
	else {
		ci  = &curr->comp_info;
	}

	*ip = curr->ip;
	*sp = curr->sp;
	return ci;
}
static inline int
chal_invstk_push(struct thread *thd, struct comp_info *ci, unsigned long ip, unsigned long sp,
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
	top->ulinvstk_off = thd->ulinvstk->top;

	return 0;
}

static inline struct comp_info *
chal_invstk_pop(struct thread *thd, unsigned long *ip, unsigned long *sp, struct cos_cpu_local_info *cos_info)
{
	struct invstk_entry *curr;

	if (unlikely(curr_invstk_top(cos_info) == 0)) return NULL;
	curr_invstk_dec(cos_info);
	return chal_invstk_current(thd, ip, sp, cos_info);
}

#endif