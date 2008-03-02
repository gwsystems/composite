/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef THREAD_H
#define THREAD_H

//#include <spd.h>
//#include <consts.h>
#include "spd.h"
#include "debug.h"
#include "consts.h"

//#include <stdio.h>

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
 * The scheduler at a specific hierarchical depth and the associated
 * thread's urgency for that scheduler (which might be an importance
 * value)
 */
struct thd_sched_info {
	struct spd *scheduler;
	int urgency;
	struct cos_sched_events *thread_notifications;
};

#define THD_STATE_PREEMPTED     0x1  /* Complete register info is saved in regs */
#define THD_STATE_UPCALL        0x2  /* Thread for upcalls: ->thd_brand points to the thread who we're branded to */
#define THD_STATE_ACTIVE_UPCALL 0x4  /* Thread is in upcall execution. */
#define THD_STATE_READY_UPCALL  0x8  /* Same as previous, but we are ready to execute */ 
#define THD_STATE_BRAND         0x10 /* This thread is used as a brand */
#define THD_STATE_SCHED_RETURN  COS_THD_SCHED_RETURN /* When the sched switches to this thread, ret from ipc */
#define THD_STATE_SCHED_EXCL    COS_SCHED_EXCL_YIELD /* The yielded thread should not be wakeable 
							by other schedulers (e.g. because it is 
							waiting for a lock) */
#define THD_STATE_FAULT         0x80 /* Thread has had a page fault which is being serviced */

/**
 * The thread descriptor.  Contains all information pertaining to a
 * thread including its address space, capabilities to services, and
 * the kernel invocation stack of execution through components.  
 */
struct thread {
	short int stack_ptr;
	unsigned short int thread_id;
	unsigned short int cpu_id, flags;

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

	void *data_region;
	vaddr_t ul_data_page;

	struct thd_sched_info sched_info[MAX_SCHED_HIER_DEPTH] CACHE_ALIGNED; 
	struct spd *sched_suspended; /* scheduler we are suspended by */

	/* flags & THD_STATE_UPCALL */
	/* The thread who's execution we are branded to */
	struct thread *thread_brand;
	/* the point in the invocation stack of the brand thread we are at */
	unsigned short int brand_inv_stack_ptr;
	struct thread *interrupted_thread;
	struct thread *upcall_threads;

	/* flags & THD_STATE_BRAND */
	unsigned long pending_upcall_requests;

	/* flags & (THD_STATE_UPCALL|THD_STATE_BRAND) != 0: */
	/* TODO singly linked list of upcall threads for a specific brand */
	//struct thread *upcall_thread_ready, *upcall_thread_active;

	struct thread *freelist_next;
} CACHE_ALIGNED;

struct cos_execution_info {
	unsigned short int thd_id, spd_id;
	//unsigned short int cpu_id;
	void *data_region;
};

struct thread *thd_alloc(struct spd *spd);
void thd_free(struct thread *thd);
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
		return MNULL; //kill the kern for now...
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

static inline struct spd *thd_curr_spd_noprint(void)
{
	struct thread *curr_thd = thd_get_current();
	unsigned int stkptr;

	if (NULL == curr_thd) return NULL;

	stkptr = curr_thd->stack_ptr;
	if (stkptr < 0) return NULL;
	
	return curr_thd->stack_base[stkptr].spd;
}

/*
 * FIXME: this is wrong.  This will give us the spd that is invoked in
 * the current composite spd, but not necessarily the _current_ spd.
 */
static inline struct spd *thd_get_thd_spd(struct thread *thd)
{
	struct thd_invocation_frame *frame = thd_invstk_top(thd);

	return frame->spd;
}

static inline struct spd_poly *thd_get_thd_spdpoly(struct thread *thd)
{
	struct thd_invocation_frame *frame = thd_invstk_top(thd);

	return frame->current_composite_spd;
}

static inline struct spd *thd_get_current_spd(void)
{
	struct thread *thd = thd_get_current();
	if (NULL == thd) { 
		return NULL;
	}
	return thd_get_thd_spd(thd);
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

static inline int thd_scheduled_by(struct thread *thd, struct spd *spd) 
{
	return thd_get_sched_info(thd, spd->sched_depth)->scheduler == spd;
}

static inline unsigned short int thd_get_id(struct thread *thd)
{
	assert(NULL != thd);

	return thd->thread_id;
}

int thd_spd_in_current_composite(struct thread *thd, struct spd *spd);

static inline struct spd *thd_validate_get_current_spd(struct thread *thd, unsigned short int spd_id)
{
	struct spd *spd = spd_get_by_index(spd_id);

	if (spd && thd_spd_in_current_composite(thd, spd)) {
		return spd;
	}

	return NULL;
}


#endif /* THREAD_H */
