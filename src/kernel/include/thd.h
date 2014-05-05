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

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
} HALF_CACHE_ALIGNED;

#define THD_INVSTK_MAXSZ 32

#ifdef LINUX_TEST

struct thread {
	thdid_t tid;
	int refcnt, invstk_top;
	cpuid_t cpuid;
	struct comp_info comp_info; /* which scheduler to notify of events? FIXME: ignored for now */
	struct invstk_entry invstk[THD_INVSTK_MAXSZ];
	/* TODO: gp and fp registers */
};

#else
#include "thread.h"
#ifndef THD_STRUCT
#define THD_STRUCT
struct thd_invocation_frame {
	struct spd_poly *current_composite_spd;
	/*
	 * sp and ip are literally the sp and ip that the kernel sets
	 * on return to user-level.
	 */
	struct spd *spd;
	vaddr_t sp, ip;
}; //HALF_CACHE_ALIGNED;

/* 
 * The scheduler at a specific hierarchical depth and the shared data
 * structure between it and this thread.
 */
struct thd_sched_info {
	struct spd *scheduler;
	struct cos_sched_events *thread_notifications;
	int notification_offset;
};

/**
 * The thread descriptor.  Contains all information pertaining to a
 * thread including its address space, capabilities to services, and
 * the kernel invocation stack of execution through components.  
 */
struct thread {
	short int stack_ptr;
	unsigned short int thread_id, cpu_id, flags;

	/* 
	 * Watch your alignments here!!!
	 *
	 * changes in the alignment of this struct must also be
	 * reflected in the alignment of regs in struct thread in
	 * ipc.S.  Would love to put this at the bottom of the struct.
	 * TODO: use offsetof to produce an include file at build time
	 * to automtically generate the assembly offsets.
	 */
        struct pt_regs regs;
        struct cos_fpu fpu;

	/* the first frame describes the threads protection domain */
	struct thd_invocation_frame stack_base[MAX_SERVICE_DEPTH] HALF_CACHE_ALIGNED;
	struct pt_regs fault_regs;

	void *data_region;
	vaddr_t ul_data_page;

	struct thd_sched_info sched_info[MAX_SCHED_HIER_DEPTH] CACHE_ALIGNED; 

	/* Start Upcall fields: */

	/* flags & THD_STATE_UPCALL */
	struct thread *interrupted_thread, *preempter_thread;

	unsigned long pending_upcall_requests;

	/* End Upcall fields */

	int cpu; /* set during creation */
	struct async_cap *srv_acap; /* The current acap the thread is waiting on. */

	/* flags & THD_STATE_UPCALL != 0: */
	//struct thread *upcall_thread_ready, *upcall_thread_active;

	struct thread *freelist_next;

//////
	thdid_t tid;
	int refcnt, invstk_top;
	cpuid_t cpuid;
	struct comp_info comp_info; /* which scheduler to notify of events? FIXME: ignored for now */
	struct invstk_entry invstk[THD_INVSTK_MAXSZ];
	/* TODO: gp and fp registers */
} CACHE_ALIGNED;
#endif

#endif

struct cap_thd {
	struct cap_header h;
	struct thread *t;
	cpuid_t cpuid;
} __attribute__((packed));

static int 
thd_activate(struct captbl *t, capid_t cap, capid_t capin, struct thread *thd, capid_t compcap)
{
	struct cap_thd *tc;
	struct cap_comp *compc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, compcap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	tc = (struct cap_thd *)__cap_capactivate_pre(t, cap, capin, CAP_THD, &ret);
	if (!tc) return ret;

	/* initialize the thread */
	memcpy(&(thd->invstk[0].comp_info), &compc->info, sizeof(struct comp_info));
	thd->invstk[0].ip = thd->invstk[0].sp = 0;
	thd->tid          = 0; /* FIXME: need correct value */
	thd->refcnt       = 0;
	thd->invstk_top   = 0;
	
	/* FIXME: 
	 * add thd->regs.ip = compc->entry_addr
	 * add parameters for the upcall and threadid/coreid/spdid etc...
	 */

	/* initialize the capability */
	tc->t     = thd;
	tc->cpuid = get_cpuid();
	__cap_capactivate_post(&tc->h, CAP_THD, 0);

	return 0;
}

static int thd_deactivate(struct captbl *t, unsigned long cap, unsigned long capin)
{ return cap_capdeactivate(t, cap, capin, CAP_THD); }

#ifdef LINUX_TEST
static void thd_init(void)
{ assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); }

extern struct thread *__thd_current;
static inline struct thread *thd_current(void) 
{ return __thd_current; }
static inline void thd_current_update(struct thread *thd)
{ __thd_current = thd; }

#else

/* void thd_init(void) */
/* { assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); } */

static inline struct thread *thd_current(void) 
{ return cos_get_curr_thd(); }
static inline void thd_current_update(struct thread *thd)
{ return cos_put_curr_thd(thd); }
#endif

static inline struct comp_info *
thd_invstk_current(struct thread *thd, unsigned long *ip, unsigned long *sp)
{
	struct invstk_entry *curr;

	/* 
	 * TODO: will be worth caching the invocation stack top along
	 * with the current thread pointer to avoid the invstk_top
	 * cacheline access.
	 */
	curr = &thd->invstk[thd->invstk_top];
	*ip = curr->ip;
	*sp = curr->sp;
	return &curr->comp_info;
}

static inline int
thd_invstk_push(struct thread *thd, struct comp_info *ci, unsigned long ip, unsigned long sp)
{
	struct invstk_entry *top, *prev;

	prev = &thd->invstk[thd->invstk_top];
	top  = &thd->invstk[thd->invstk_top+1];
	if (unlikely(thd->invstk_top >= THD_INVSTK_MAXSZ)) return -1;
	thd->invstk_top++;
	prev->ip = ip;
	prev->sp = sp;
	memcpy(&top->comp_info, ci, sizeof(struct comp_info));
	top->ip  = top->sp = 0;

	return 0;
}

static inline struct comp_info *
thd_invstk_pop(struct thread *thd, unsigned long *ip, unsigned long *sp)
{
	if (unlikely(thd->invstk_top == 0)) return NULL;
	thd->invstk_top--;
	return thd_invstk_current(thd, ip, sp);
}

#endif /* THD_H */
