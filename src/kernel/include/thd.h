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

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
} HALF_CACHE_ALIGNED;

#define THD_INVSTK_MAXSZ 32

#define THD_STATE_PREEMPTED     0x1   /* Complete register info is saved in regs */
#define THD_STATE_UPCALL        0x2   /* Thread for upcalls: ->srv_acap points to the acap who we're linked to */
#define THD_STATE_ACTIVE_UPCALL 0x4   /* Thread is in upcall execution. */
#define THD_STATE_READY_UPCALL  0x8   /* Same as previous, but we are ready to execute */ 
#define THD_STATE_SCHED_RETURN  0x10  /* When the sched switches to this thread, ret from ipc */
#define THD_STATE_FAULT         0x20  /* Thread has had a (e.g. page) fault which is being serviced */
#define THD_STATE_HW_ACAP      0x40 /* Actual hardware should be making this acap */
#define THD_STATE_CYC_CNT       0x80 /* This thread is being cycle tracked */

#ifdef LINUX_TEST

//used for tests only
struct thread {
	thdid_t tid;
	int refcnt, invstk_top;
	cpuid_t cpuid;
	struct comp_info comp_info; /* which scheduler to notify of events? FIXME: ignored for now */
	struct invstk_entry invstk[THD_INVSTK_MAXSZ];
////////////////
        struct pt_regs regs;
	unsigned short int thread_id, cpu_id, flags;
	struct thread *interrupted_thread, *preempter_thread;
	capid_t arcv_cap; /* the acap id we are waiting on */
};

#else

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
	capid_t arcv_cap; /* the acap id we are waiting on */
	/* TODO: gp and fp registers */
} CACHE_ALIGNED;

#endif

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
alloc_thd_id(void)
{
        /* FIXME: thd id address space management. */
	if (unlikely(free_thd_id >= MAX_NUM_THREADS)) assert(0);
	return cos_faa((int*)&free_thd_id, 1);
}

static int 
thd_activate(struct captbl *t, capid_t cap, capid_t capin, struct thread *thd, capid_t compcap, int init_data)
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
	thd->tid          = alloc_thd_id();
	thd->refcnt       = 1;
	thd->invstk_top   = 0;
	assert(thd->tid <= MAX_NUM_THREADS);
	
	thd_upcall_setup(thd, compc->entry_addr, 
			 COS_UPCALL_THD_CREATE, init_data, 0, 0);

	/* initialize the capability */
	tc->t      = thd;
	thd->cpuid = tc->cpuid = get_cpuid();
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
		/* Last reference. Require pgtbl and
		 * cos_frame cap to release the kmem
		 * page. */
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

#ifdef LINUX_TEST
static void thd_init(void)
{ assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); }

extern struct thread *__thd_current;

static inline struct thread *
thd_current(void *ignore) 
{ (void)ignore; return __thd_current; }

static inline void 
thd_current_update(struct thread *thd, struct thread *ignore)
{ (void)ignore; __thd_current = thd; }

#else

static void 
thd_init(void)
{ assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); }

static inline struct thread *
thd_current(struct cos_cpu_local_info *cos_info) 
{ return (struct thread *)(cos_info->curr_thd); }

static inline void 
thd_current_update(struct thread *next, struct thread *prev, struct cos_cpu_local_info *cos_info)
{
	/* commit the cached data */
	prev->invstk_top = cos_info->invstk_top;
	cos_info->invstk_top = next->invstk_top;

	cos_info->curr_thd = (void *)next;
}
#endif

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

static inline void thd_preemption_state_update(struct thread *curr, struct thread *next, struct pt_regs *regs)
{
	curr->flags |= THD_STATE_PREEMPTED;
	curr->preempter_thread = next;
	next->interrupted_thread = curr;
	memcpy(&curr->regs, regs, sizeof(struct pt_regs));
}


#endif /* THD_H */
