/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

//#include <ipc.h>
//#include <spd.h>
#include "include/ipc.h"
#include "include/spd.h"
#include "include/debug.h"

//#include <stdio.h>
//#include <malloc.h>

#include <linux/kernel.h>

void print(void)
{
	printd("cos: (*)\n");
}

void print_val(unsigned int val)
{
	printd("cos: (%x)\n", val);
}


/* typedef int (*fn_t)(void); */
/* void *kern_stack; */
/* void *kern_stack_addr; */

/* /\* The user-level representation of an spd's caps *\/ */
/* struct user_inv_cap *ST_user_caps; */

void ipc_init(void)
{
//	kern_stack_addr = kmalloc(PAGE_SIZE);
//	kern_stack = kern_stack_addr+(PAGE_SIZE-sizeof(void*))/sizeof(void*);

	return;
}

static inline void open_spd(struct spd_poly *spd)
{
	printk("cos: open_spd (asymmetric trust) not supported on x86.\n");
	
	return;
}

static inline void open_close_spd(struct spd_poly *o_spd,
				  struct spd_poly *c_spd)
{
	native_write_cr3(o_spd->pg_tbl);
//	printk("cos: invocation - opening pgtbl %x for spd %p.\n", (unsigned int)o_spd->pg_tbl, o_spd);
		
	return;
}

static inline void open_close_spd_ret(struct spd_poly *c_spd) /*, struct spd_poly *s_spd)*/
{
	native_write_cr3(c_spd->pg_tbl);
//	printk("cos: return - opening pgtbl %x for spd %p.\n", (unsigned int)o_spd->pg_tbl, o_spd);
	
	return;
}

static inline int stale_il_or_error(struct thd_invocation_frame *frame,
				    struct invocation_cap *cap)
{
	return 1;
}

static void print_stack(struct thread *thd, struct spd *srcspd, struct spd *destspd)
{
	int i;

	printk("cos: In thd %x, src spd %x, dest spd %x, stack:\n", (unsigned int)thd, 
	       (unsigned int)srcspd, (unsigned int)destspd);
	for (i = 0 ; i <= thd->stack_ptr ; i++) {
		struct thd_invocation_frame *frame = &thd->stack_base[i];
		printk("cos: \t[cspd %x]\n", (unsigned int)frame->current_composite_spd);
	}
}

static void print_regs(struct pt_regs *regs)
{
	printk("cos: EAX:%x\tEBX:%x\tECX:%x\n"
	       "cos: EDX:%x\tESI:%x\tEDI:%x\n"
	       "cos: EIP:%x\tESP:%x\tEBP:%x\n",
	       (unsigned int)regs->eax, (unsigned int)regs->ebx, (unsigned int)regs->ecx,
	       (unsigned int)regs->edx, (unsigned int)regs->esi, (unsigned int)regs->edi,
	       (unsigned int)regs->eip, (unsigned int)regs->esp, (unsigned int)regs->ebp);

	return;
}

struct inv_ret_struct {
	int thd_id;
	void *data_region;
};

extern struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];
/* 
 * FIXME: 1) should probably return the static capability to allow
 * isolation level isolation access from caller, 2) all return 0
 * should kill thread.
 */
__attribute__((regparm(0)))
vaddr_t ipc_walk_static_cap(struct thread *thd, unsigned int capability, 
			    vaddr_t sp, vaddr_t ip, vaddr_t usr_def, 
			    struct inv_ret_struct *ret)
{
	struct thd_invocation_frame *curr_frame;
	struct spd *curr_spd, *dest_spd;
	struct invocation_cap *cap_entry;
//	int save_regs = 0;

/* FIXME: unify with asm_ipc_defs.h */
/*
#define SAVE_REGS_CAP_OFFSET 0x80000000
	if (SAVE_REGS_CAP_OFFSET & capability) {
		save_regs = 1;
		capability &= ~SAVE_REGS_CAP_OFFSET;
	}
*/
	capability >>= 20;

	//printd("cos: ipc_walk_static_cap - thd %p, cap %x(%u), sp %x, ip %x, usrdef %x.\n",
	//       thd, (unsigned int)capability, (unsigned int)capability, 
	//       (unsigned int)sp, (unsigned int)ip, (unsigned int)usr_def);

	if (capability >= MAX_STATIC_CAP) {
		printk("cos: capability %d greater than max.\n", capability);
		return 0;
	}

	cap_entry = &invocation_capabilities[capability];

	if (!cap_entry->owner) {
		printk("cos: No owner for cap %d.\n", capability);
		return 0;
	}

	//printk("cos: \tinvoking fn %x.\n", (unsigned int)cap_entry->dest_entry_instruction);

	/* what spd are we in (what stack frame)? */
	curr_frame = &thd->stack_base[thd->stack_ptr];

	dest_spd = cap_entry->destination;
	curr_spd = cap_entry->owner;

	if (curr_spd == CAP_FREE || curr_spd == CAP_ALLOCATED_UNUSED) {
		printk("cos: Attempted use of unallocated capability.\n");
		return 0;
	}

	/*printk("cos: Invocation on cap %d from %x.\n", capability, 
	       (unsigned int)curr_frame->current_composite_spd);*/
	/*
	 * If the spd that owns this capability is part of a composite
	 * spd that is the same as the composite spd that was the
	 * entry point for this composite spd.
	 *
	 * i.e. is the capability owner in the same protection domain
	 * (via ST) as the spd entry point to the protection domain.
	 */
	if (curr_spd->composite_spd != curr_frame->current_composite_spd && /* should be || */
	    stale_il_or_error(curr_frame, cap_entry)) {
		printk("cos: Error, incorrect capability (Cap %d has cspd %x, stk has %x).\n",
		       capability, (unsigned int)cap_entry->owner->composite_spd,
		       (unsigned int)curr_frame->current_composite_spd);
		print_stack(thd, curr_spd, dest_spd);
		return 0;
		/* FIXME: do something here like kill thread/spd */
	}

	/* save the proper spd that we saved registers for */
/*	if (save_regs) {
		thd->sched_saved_regs = curr_spd;
	}
*/

	cap_entry->invocation_cnt++;

//	if (cap_entry->il & IL_INV_UNMAP) {
		open_close_spd(&dest_spd->composite_spd->spd_info, 
			       &curr_spd->composite_spd->spd_info);
//	} else {
//		open_spd(&curr_spd->spd_info);
//	}

	ret->thd_id = thd->thread_id;
	ret->data_region = thd->data_region;

	/* 
	 * ref count the composite spds:
	 * 
	 * FIXME, TODO: move composite pgd into each spd and ref count
	 * in spds.  Sum of all ref cnts is the composite ref count.
	 * This will eliminate the composite cache miss.
	 */
	
	/* add a new stack frame for the spd we are invoking (we're committed) */
	thd_invocation_push(thd, cap_entry->destination->composite_spd, sp, ip, usr_def);

	return cap_entry->dest_entry_instruction;
}

__attribute__((regparm(0)))
struct thd_invocation_frame *pop(struct thread *curr_thd)
{
	struct thd_invocation_frame *inv_frame = thd_invocation_pop(curr_thd);
	struct thd_invocation_frame *curr_frame;

	if (inv_frame == MNULL) {
		printk("Attempting to return from a component when there's no component to return to.\n");
		return MNULL;
	}
	
	//printk("cos: Popping spd %p off of thread %p.\n", 
	//       &inv_frame->current_composite_spd->spd_info, curr_thd);
	
	curr_frame = thd_invstk_top(curr_thd);
	/* for now just assume we always close the server spd */
	open_close_spd_ret(&curr_frame->current_composite_spd->spd_info);

	return inv_frame;	
}

/********** Composite system calls **********/

extern int switch_thread_data_page(int old_thd, int new_thd);
void switch_thread_context(struct thread *curr, struct thread *next)
{
	thd_set_current(next);

	switch_thread_data_page(curr->thread_id, next->thread_id);

	/* 
	 * TODO: with preemption, check if curr_spd for next is
	 * curr_spc for curr...if not, change page tables. 
	 */

	return;
}

__attribute__((regparm(0)))
int cos_syscall_void(void)
{
	printd("cos: error - made void system call\n");

	return 0;
}

extern void cos_syscall_resume_return(void);
/*
 * Called from assembly so error conventions are ugly:
 * 0: error
 * 1: return from invocation
 * 2: return from preemption
 *
 * Does this have to be so expensive?  400 cycles above normal
 * invocation return.
 */
__attribute__((regparm(0)))
int cos_syscall_resume_return_cont(int thd_id)
{
	struct thread *thd = thd_get_by_id(thd_id), *curr = thd_get_current();
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();

	if (thd == NULL || curr == NULL) {
		printk("cos: no thread associated with id %d.\n", thd_id);
		return 0;
	}

	if (curr_spd == NULL || curr_spd->sched_depth == -1) {
		printk("cos: spd invoking resume thread not a scheduler.\n");
		return 0;
	}

	if (thd->sched_suspended && curr_spd != thd->sched_suspended) {
		printk("cos: scheduler %x resuming thread %d (%x), not one that suspended it %x.\n",
		       (unsigned int)curr_spd, thd_id, (unsigned int)thd, (unsigned int)thd->sched_suspended);
		return 0;
	}

	curr->sched_suspended = curr_spd;
	thd->sched_suspended = NULL;

	switch_thread_context(curr, thd);

	if (thd->flags & THD_STATE_PREEMPTED) {
		printk("cos: resume preempted -- disabled path...why are we here?\n");
		thd->flags &= ~THD_STATE_PREEMPTED;
		return 2;
	} 
	
	return 1;
}

__attribute__((regparm(0)))
int cos_syscall_get_thd_id(void)
{
	//printk("cos: request for thread is %d.\n", thd_get_current()->thread_id);
	return thd_get_current()->thread_id;
}

/* 
 * Hope the current thread saved its context...should be able to
 * resume_return to it.
 */
__attribute__((regparm(0)))
int cos_syscall_create_thread(vaddr_t fn, vaddr_t stack, void *data)
{
	struct thread *thd;//, *curr;
	/* FIXME: as above, cast to spd stupid */
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();
	
	thd = thd_alloc(curr_spd);
	if (thd == NULL) {
		return -1;
	}

//	printk("cos: stack %x, fn %x, and data %p\n", stack, fn, data);
	thd->regs.ecx = stack;
	thd->regs.edx = fn;
	thd->regs.eax = (int)data;

//	printk("cos: allocated thread %d.\n", thd->thread_id);
	/* TODO: set information in the thd such as schedulers[], etc... from the parent */

	//curr = thd_get_current();

	/* Suspend the current thread */
	//curr->sched_suspended = curr_spd;
	/* and activate the new one */
	//switch_thread_context(curr, thd);

	//printk("cos: switching from thread %d to %d.\n", curr->thread_id, thd->thread_id);

	//print_regs(&curr->regs);

	return thd->thread_id;
}

extern int cos_syscall_switch_thread(int thd_id);
__attribute__((regparm(0)))
struct pt_regs *cos_syscall_switch_thread_cont(int thd_id)
{
	struct thread *thd = thd_get_by_id(thd_id), *curr = thd_get_current();
	/* FIXME: cast disregards spd_poly, which will break mpd */
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();

	if (thd == NULL || curr == NULL) {
		printk("cos: no thread associated with id %d.\n", thd_id);
		return &curr->regs;
	}

	if (/*curr_spd == NULL || */curr_spd->sched_depth == -1) {
		printk("cos: spd invoking resume thread not a scheduler.\n");
		return &curr->regs;
	}

	/*
	 * If the thread was suspended by another scheduler, we really
	 * have no business resuming it.
	 */
	if (thd->sched_suspended && curr_spd != thd->sched_suspended) {
		printk("cos: scheduler %x resuming thread %d (%x), not one that suspended it %x.\n",
		       (unsigned int)curr_spd, thd_id, (unsigned int)thd, (unsigned int)thd->sched_suspended);
		return &curr->regs;
	}

	/*
	 * TODO: error if thd->schedulers[curr_spd->sched_depth] != curr_spd, for curr too
	 */

	curr->sched_suspended = curr_spd;
	thd->sched_suspended = NULL;

	switch_thread_context(curr, thd);

//	print_regs(&thd->regs);

	/* not yet...
	if (thd->flags & THD_STATE_PREEMPTED) {
		printk("cos: resume preempted -- disabled path...why are we here?\n");
		thd->flags &= ~THD_STATE_PREEMPTED;
		return 2;
	} 
	*/
	
	return &thd->regs;
}

extern void cos_syscall_kill_thd(int thd_id);
__attribute__((regparm(0)))
void cos_syscall_kill_thd_cont(int thd_id)
{
	printk("cos: killing threads not yet supported.\n");

	return;

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
 * Go back to the design proposed in the design document earlier.
 */

//extern void cos_syscall_brand_upcall(int thread_id);
__attribute__((regparm(0)))
int cos_syscall_brand_upcall(int thread_id)
{
	return 0;
}

__attribute__((regparm(0)))
int cos_syscall_create_brand(void)
{
	return 0;
}

/* 
 * I HATE this call...do away with it if possible.  But we need some
 * way to jump-start processes AND let schedulers keep track of their
 * threads.
 *
 * NOT performance sensitive: used to kick start spds and give them
 * active entities (threads).
 */
extern void cos_syscall_upcall(int thd_id);
__attribute__((regparm(0)))
int cos_syscall_upcall_cont(int spd_id, vaddr_t *inv_addr)
{
	struct spd *dest = spd_get_by_index(spd_id);
	/* FIXME: cast disregards spd_poly, which will break mpd */
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();
	struct thread *thd = thd_get_current();

	/*
	 * FIXME: credibility/validity testing of this upcall is not
 	 * done.  Lookup cap range for dest spd and make sure that
	 * curr_spd is in at least one capability.
	 */

	if (dest == NULL || curr_spd == NULL) {
		printk("cos: upcall attempt failed - dest_spd = %p, curr_spd = %p.\n",
		       dest, curr_spd);
		
		return -1;
	}

	open_close_spd(&dest->composite_spd->spd_info,
		       &curr_spd->composite_spd->spd_info);
	
	/* set the thread to have a new base owner */
	thd->stack_ptr = 0;
	thd->stack_base[0].current_composite_spd = (struct composite_spd*)dest;

	*inv_addr = dest->upcall_entry;

	return thd->thread_id;
}

void *cos_syscall_tbl[16] = {
	(void*)cos_syscall_void,
	(void*)cos_syscall_resume_return,
	(void*)cos_syscall_get_thd_id,
	(void*)cos_syscall_create_thread,
	(void*)cos_syscall_switch_thread,
	(void*)cos_syscall_kill_thd,
	(void*)cos_syscall_brand_upcall,
	(void*)cos_syscall_create_brand,
	(void*)cos_syscall_upcall,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
};
