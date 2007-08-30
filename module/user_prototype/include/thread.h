/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#ifndef THREAD_H
#define THREAD_H

#include <spd.h>
#include <consts.h>
//#include <regs.h>

#include <stdio.h>

/* 
 * There are 4 thread descriptors per page: the thread_t is 16 bytes,
 * each stack frame is the same, and we define MAX_SERVICE_DEPTH to be
 * 31, making a total of 512 bytes.  We then reserve 512 bytes for
 * kernel stack usage.
 */

#define MAX_SERVICE_DEPTH 31
#define MAX_NUM_THREADS 4
#define THREAD_SIZE PAGE_SIZE//sizeof(struct thread)

/*
#if (PAGE_SIZE % THREAD_SIZE != 0)
#error "Page size must be multiple of thread size."
#endif
*/

typedef struct thd_invocation_frame {
	struct composite_spd *current_composite_spd;
	/*
	 * ret_addr is stored and passed up to the user-level on
	 * return: it allows that code to know where to return to.  sp
	 * and ip are literally the sp and ip that the kernel sets on
	 * return to user-level.
	 */
	vaddr_t usr_def, sp, ip;
} HALF_CACHE_ALIGN;

/**
 * The thread descriptor.  Contains all information pertaining to a
 * thread including its address space, capabilities to services, and
 * the kernel stack.  This should be page aligned and the rest of the
 * page should be devoted to the kernel stack.
 */
typedef struct thread {
	unsigned short int stack_ptr, thread_id;
	unsigned short int cpu_id, urgency;

	/*gp_regs_t*/int *regs;

	struct thread *suspended_next;
	struct thread *freelist_next;

	/* the first frame describes the threads protection domain */
	struct thd_invocation_frame stack_base[MAX_SERVICE_DEPTH] HALF_CACHE_ALIGNED;
} CACHE_ALIGN;

struct thread *thd_alloc();
void thd_init(void);
void thd_set_current(struct thread *thd);

/*
 * Bounds checking is not done here as we statically guarantee that
 * the maximum diameter of services is less than the size of the
 * invocation stack.
 */
static inline void thd_invocation_push(struct thread *curr_thd, 
				       struct composite_spd *curr_composite, 
				       vaddr_t sp, vaddr_t ip, vaddr_t usr_def)
{
	struct thd_invocation_frame *inv_frame;
/*
	printf("Pushing onto %x, cspd %x (sp %x, ip %x, usr %x).\n", (unsigned int)curr_thd,
	       (unsigned int)curr_composite, (unsigned int)sp, (unsigned int)ip, (unsigned int)usr_def);
*/
	curr_thd->stack_ptr++;
	inv_frame = &curr_thd->stack_base[curr_thd->stack_ptr];

	inv_frame->current_composite_spd = curr_composite;
	inv_frame->sp = sp;
	inv_frame->ip = ip;
	inv_frame->usr_def = usr_def;

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

	if (curr_thd->stack_ptr == 0) {
		printf("Tried to return when there is nowhere to return to.\n");
		return MNULL;
		/* FIXME: kill the thread */
	}

	prev_frame = &curr_thd->stack_base[curr_thd->stack_ptr];
	curr_thd->stack_ptr--;
/*
	printf("Popping off of %x, cspd %x (sp %x, ip %x, usr %x).\n", (unsigned int)curr_thd,
	       (unsigned int)prev_frame->current_composite_spd, (unsigned int)prev_frame->sp, (unsigned int)prev_frame->ip, (unsigned int)prev_frame->usr_def);
	fflush(stdout);
*/
	return prev_frame;
}

#endif /* THREAD_H */
