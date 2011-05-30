/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef THREAD_H
#define THREAD_H

#include "spd.h"
#include "debug.h"
#include "shared/consts.h"

#include <linux/kernel.h>

/* 
 * There are 4 thread descriptors per page: the thread_t is 16 bytes,
 * each stack frame is the same, and we define MAX_SERVICE_DEPTH to be
 * 31, making a total of 512 bytes.  We then reserve 512 bytes for
 * kernel stack usage.
 */

/*
#if (PAGE_SIZE % THREAD_SIZE != 0)
#error "Page size must be multiple of thread size."
#endif
*/

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

#define THD_STATE_PREEMPTED     0x1   /* Complete register info is saved in regs */
#define THD_STATE_UPCALL        0x2   /* Thread for upcalls: ->thd_brand points to the thread who we're branded to */
#define THD_STATE_ACTIVE_UPCALL 0x4   /* Thread is in upcall execution. */
#define THD_STATE_READY_UPCALL  0x8   /* Same as previous, but we are ready to execute */ 
#define THD_STATE_BRAND         0x10  /* This thread is used as a brand */
#define THD_STATE_SCHED_RETURN  0x20  /* When the sched switches to this thread, ret from ipc */
#define THD_STATE_FAULT         0x40  /* Thread has had a (e.g. page) fault which is being serviced */
#define THD_STATE_HW_BRAND      0x80 /* Actual hardware should be making this brand */
#define THD_STATE_CYC_CNT       0x100 /* This thread is being cycle tracked */

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

	/* the first frame describes the threads protection domain */
	struct thd_invocation_frame stack_base[MAX_SERVICE_DEPTH] HALF_CACHE_ALIGNED;
	struct pt_regs fault_regs;

	void *data_region;
	vaddr_t ul_data_page;

	struct thd_sched_info sched_info[MAX_SCHED_HIER_DEPTH] CACHE_ALIGNED; 

	/* Start Brand & Upcall fields: */

	/* flags & THD_STATE_UPCALL */
	/* The thread who's execution we are branded to */
	struct thread *thread_brand;
	struct thread *interrupted_thread, *preempter_thread;
	struct thread *upcall_threads;

	/* flags & THD_STATE_BRAND */
	unsigned long pending_upcall_requests;

	/* HACK: recv ring buffer for network packets, both user-level
	 * and kernel-level pointers
	 */
	ring_buff_t *u_rb, *k_rb;
	int rb_next; 		/* Next address entry */

	/* End Brand & Upcall fields */

	/* flags & (THD_STATE_UPCALL|THD_STATE_BRAND) != 0: */
	/* TODO singly linked list of upcall threads for a specific brand */
	//struct thread *upcall_thread_ready, *upcall_thread_active;

	struct thread *freelist_next;
} CACHE_ALIGNED;

struct thread *thd_alloc(struct spd *spd);
void thd_free(struct thread *thd);
void thd_free_all(void);
void thd_init(void);

/*
 * Bounds checking is not done here as we statically guarantee that
 * the maximum diameter of services is less than the size of the
 * invocation stack.
 */
static inline void thd_invocation_push(struct thread *curr_thd, struct spd *curr_spd,
				       vaddr_t sp, vaddr_t ip)
{
	struct thd_invocation_frame *inv_frame;
/*
	printk("cos: Pushing onto %p, spd %p, cspd %p (sp %x, ip %x).\n", 
	       curr_thd, curr_spd, curr_spd->composite_spd, 
	       (unsigned int)sp, (unsigned int)ip);
*/
	curr_thd->stack_ptr++;
	inv_frame = &curr_thd->stack_base[curr_thd->stack_ptr];

	inv_frame->current_composite_spd = curr_spd->composite_spd;
	inv_frame->sp = sp;
	inv_frame->ip = ip;
	inv_frame->spd = curr_spd;

	return;
}

//extern struct user_inv_cap *ST_user_caps;
/* 
 * The returned invocation frame will be invalid if a push happens,
 * ie. the returned pointer is to an item on the stack.
 */
static inline struct thd_invocation_frame *thd_invocation_pop(struct thread *curr_thd)
{
	struct thd_invocation_frame *prev_frame;

	if (curr_thd->stack_ptr <= 0) {
		//printd("Tried to return without invocation.\n");
		/* FIXME: kill the thread if not a branded upcall thread */
		return NULL; //kill the kern for now...
	}

	prev_frame = &curr_thd->stack_base[curr_thd->stack_ptr--];
/*
	printd("Popping off of %x, cspd %x (sp %x, ip %x, usr %x).\n", (unsigned int)curr_thd,
	       (unsigned int)prev_frame->current_composite_spd, (unsigned int)prev_frame->sp, (unsigned int)prev_frame->ip, (unsigned int)prev_frame->usr_def);
*/
	return prev_frame;
}

static inline struct thd_invocation_frame *thd_invstk_top(struct thread *curr_thd)
{
	/* pop should not allow us to escape from our home spd */
	//assert(curr_thd->stack_ptr >= 0);

	if (curr_thd->stack_ptr < 0) return NULL;
	
	return &curr_thd->stack_base[curr_thd->stack_ptr];
}

static inline struct thd_invocation_frame *thd_invstk_nth(struct thread *thd, int nth)
{
	int idx = thd->stack_ptr - nth;

	if (idx < 0) return NULL;

	return &thd->stack_base[idx];
}

static inline void thd_invstk_move_nth(struct thread *thd, int nth, int rem)
{
	struct thd_invocation_frame *if_keep, *if_rem;

	assert(nth != 0);
	assert(nth < thd->stack_ptr);

	if_keep = &thd->stack_base[nth+1];
	if_rem = &thd->stack_base[nth];
	if (rem) {
		if_rem->spd = if_keep->spd;
		if_rem->current_composite_spd = if_keep->current_composite_spd;
	} else {
		memcpy(if_rem, if_keep, sizeof(struct thd_invocation_frame));
	}
}

static inline int thd_invstk_rem_nth(struct thread *thd, unsigned int nth)
{
	int idx = thd->stack_ptr - nth, i;
	struct spd_poly *cspd;
	int first = 1;

	if (nth >= thd->stack_ptr) return -1;
	/* release the composite spd */
	cspd = thd->stack_base[idx].current_composite_spd;
	spd_mpd_ipc_release((struct composite_spd*)cspd);
	/* shift the stack items down by one */
	for (i = idx ; i < thd->stack_ptr ; i++) {
		thd_invstk_move_nth(thd, i, first);
		first = 0;
	}
	thd->stack_ptr--;

	return 0;
}

extern struct thread *current_thread;
static inline struct thread *thd_get_current(void) 
{
	return current_thread;
}

static inline void thd_set_current(struct thread *thd)
{
	current_thread = thd;

	return;
}

static inline struct spd *
thd_curr_spd_thd(struct thread *t)
{
	unsigned int stkptr;

	if (NULL == t) return NULL;

	stkptr = t->stack_ptr;
	if (stkptr >= MAX_SERVICE_DEPTH) return NULL;
	
	return t->stack_base[stkptr].spd;
}

static inline struct spd *thd_curr_spd_noprint(void)
{
	return thd_curr_spd_thd(thd_get_current());
}

static inline vaddr_t thd_get_frame_ip(struct thread *thd, int frame_offset)
{
	int off;
	/* 
	 * The only weird thing here is that we are looking at the
	 * ip/sp stored in the frame_offset + 1.  This is because the
	 * ip/sp is stored for the previous protection domain in the
	 * stack frame of the current (much like the ip on a c call
	 * stack is at the beginning of the next call's stack frame.
	 */

	if (frame_offset > thd->stack_ptr) return 0;
	off = thd->stack_ptr - frame_offset;
	if (off == 0) {
		if (thd->flags & THD_STATE_PREEMPTED) {
			return thd->regs.ip;
		} else {
			return thd->regs.dx;
		}
	} else {
		struct thd_invocation_frame *tif;
		tif = &thd->stack_base[off+1];
		return tif->ip;
	}
}

static inline vaddr_t thd_set_frame_ip(struct thread *thd, int frame_offset, unsigned long new_ip)
{
	int off;

	if (frame_offset > thd->stack_ptr) return -1;
	off = thd->stack_ptr - frame_offset;
	if (off == 0) {
		if (thd->flags & THD_STATE_PREEMPTED) {
			thd->regs.ip = new_ip;
		} else {
			thd->regs.dx = new_ip;
		}
	} else {
		struct thd_invocation_frame *tif;
		tif = &thd->stack_base[off+1];
		tif->ip = new_ip;
	}
	return 0;
}

static inline vaddr_t thd_get_frame_sp(struct thread *thd, int frame_offset)
{
	int off;
	/* See comment in thd_get_frame_ip */

	if (frame_offset > thd->stack_ptr) return 0;
	off = thd->stack_ptr - frame_offset;
	if (off == 0) {
		if (thd->flags & THD_STATE_PREEMPTED) {
			return thd->regs.sp;
		} else {
			return thd->regs.cx;
		}
	} else {
		struct thd_invocation_frame *tif;
		tif = &thd->stack_base[off+1];
		return tif->sp;
	}
}

static inline vaddr_t thd_set_frame_sp(struct thread *thd, int frame_offset, unsigned long new_sp)
{
	int off;
	/* See comment in thd_get_frame_ip */

	if (frame_offset > thd->stack_ptr) return 1;
	off = thd->stack_ptr - frame_offset;
	if (off == 0) {
		if (thd->flags & THD_STATE_PREEMPTED) {
			thd->regs.sp = new_sp;
		} else {
			thd->regs.cx = new_sp;
		}
	} else {
		struct thd_invocation_frame *tif;
		tif = &thd->stack_base[off+1];
		tif->sp = new_sp;
	}
	return 0;
}

static inline vaddr_t thd_get_ip(struct thread *t)
{
	return thd_get_frame_ip(t, t->stack_ptr);
}

static inline vaddr_t thd_get_sp(struct thread *t)
{
	return thd_get_frame_sp(t, t->stack_ptr);
}

/* 
 * Important: this call should not be used in functions to carry out
 * security/authorization checks.  If the 
 *
 * For such a simple call, this has a lot of baggage: getting the
 * entry component of the top frame will not give an accurate
 * "resident component" if we are using mpd merge.  Instead we'll use
 * the instruction pointer and find out which component it lies in.
 */
static inline struct spd *thd_get_thd_spd(struct thread *thd)
{
	struct thd_invocation_frame *frame;
	struct spd *spd;

	spd = virtual_namespace_query(thd_get_ip(thd));
	if (spd) return spd;
	/* thd_get_thd_spd is not used in sec */
	frame = thd_invstk_top(thd);
	return frame->spd;
}

static inline struct spd_poly *thd_get_thd_spdpoly(struct thread *thd)
{
	struct thd_invocation_frame *frame = thd_invstk_top(thd);

	return frame->current_composite_spd;
}

static inline struct spd_poly *thd_get_current_spdpoly(void)
{
	struct thread *thd = thd_get_current();
	if (NULL == thd) { 
		return NULL;
	}
	return thd_get_thd_spdpoly(thd);
}

extern struct thread threads[MAX_NUM_THREADS];
static inline struct thread *thd_get_by_id(int id)
{
	/* Thread 0 is reserved. */
	int adjusted = id-1;

 	if (adjusted >= MAX_NUM_THREADS || adjusted < 0) 
		return NULL;

	return &threads[adjusted];
}

static inline struct thd_sched_info *thd_get_sched_info(struct thread *thd, 
							unsigned short int depth)
{
	assert(depth < MAX_SCHED_HIER_DEPTH);

	return &thd->sched_info[depth];
}

static inline unsigned short int thd_get_depth_urg(struct thread *t,
						   unsigned short int depth)
{
	struct thd_sched_info *tsi;
	struct spd *sched;

	tsi = thd_get_sched_info(t, depth);
	sched = tsi->scheduler;
	assert(sched);
	assert(tsi->thread_notifications);

	return COS_SCHED_EVT_URGENCY(tsi->thread_notifications);
}

static inline struct spd *thd_get_depth_sched(struct thread *t, unsigned short int d)
{
	return thd_get_sched_info(t, d)->scheduler;
}

static inline int thd_scheduled_by(struct thread *thd, struct spd *spd) 
{
	return thd_get_sched_info(thd, spd->sched_depth)->scheduler == spd;
}

static inline unsigned short int thd_get_id(struct thread *thd)
{
	assert(NULL != thd);

	return thd->thread_id;
}

static inline int thd_spd_in_composite(struct spd_poly *comp, struct spd *spd)
{
	/*
	 * First we check the cache: spd->composite_spd which will
	 * point to the composite spd that the spd is currently part
	 * of.  It is very likely that we will hit in this cache
	 * unless we are using a stale mapping.  In such a case, we
	 * must look in the page table of the composite and see if the
	 * spd is present.  This is significantly more expensive.
	 */
	return spd->composite_spd == comp || spd_composite_member(spd, comp);
}

static inline int thd_spd_in_current_composite(struct thread *thd, struct spd *spd)
{
	struct spd_poly *composite = thd_get_thd_spdpoly(thd);

	return thd_spd_in_composite(composite, spd);
}

static inline struct spd *thd_validate_get_current_spd(struct thread *thd, unsigned short int spd_id)
{
	struct spd *spd = spd_get_by_index(spd_id);

	if (spd && thd_spd_in_current_composite(thd, spd)) return spd;

	return NULL;
}

int thd_validate_spd_in_callpath(struct thread *t, struct spd *s);
int thd_check_atomic_preempt(struct thread *thd);
void thd_print_regs(struct thread *t);

static inline void thd_save_preempted_state(struct thread *thd, struct pt_regs *regs)
{
	/* preempt and save current thread */
	memcpy(&thd->regs, regs, sizeof(struct pt_regs));
	//thd->flags |= THD_STATE_PREEMPTED;
	//printk("cos: preempting thread %d with regs:\n", thd_get_id(thd));
	//thd_print_regs(thd);
}

#endif /* THREAD_H */
