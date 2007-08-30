/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#include <ipc.h>
#include <spd.h>

#include <stdio.h>
#include <malloc.h>


void print(void)
{
	printf("(*)\n");
	fflush(stdout);
}

void print_val(unsigned int val)
{
	printf("(%x)\n", val);
	fflush(stdout);
}


typedef int (*fn_t)(void);
void *kern_stack;
void *kern_stack_addr;

/* The user-level representation of an spd's caps */
struct user_inv_cap *ST_user_caps;

void ipc_init(void)
{
	kern_stack_addr = malloc(PAGE_SIZE);
	kern_stack = kern_stack_addr+(PAGE_SIZE-sizeof(void*))/sizeof(void*);

	return;
}

static inline void open_spd(struct spd_poly *spd)
{
	return;
}

static inline void open_close_spd(struct spd_poly *o_spd,
				  struct spd_poly *c_spd)
{
	open_spd(o_spd);

	return;
}

static int stale_il_or_error(struct thd_invocation_frame *frame,
			     struct invocation_cap *cap)
{
	return 1;
}

static void print_stack(struct thread *thd, struct spd *srcspd, struct spd *destspd)
{
	int i;

	printf("In thd %x, src spd %x, dest spd %x:\n", (unsigned int)thd, 
	       (unsigned int)srcspd, (unsigned int)destspd);
	for (i = 0 ; i <= thd->stack_ptr ; i++) {
		struct thd_invocation_frame *frame = &thd->stack_base[i];
		printf("[cspd %x]\n", (unsigned int)frame->current_composite_spd);
	}
}

extern struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];
/* 
 * FIXME: should probably return the static capability to allow
 * isolation level isolation access from caller.
 */
fn_t ipc_walk_static_cap(struct thread *thd, int capability, 
			 vaddr_t sp, vaddr_t ip, vaddr_t usr_def)
{
	struct thd_invocation_frame *curr_frame;
	struct spd *curr_spd, *dest_spd;
	struct invocation_cap *cap_entry;

	// capability &= 0xFFFF; 
	cap_entry = &invocation_capabilities[capability];

	/* what spd are we in (what stack frame)? */
	curr_frame = &thd->stack_base[thd->stack_ptr];

	dest_spd = cap_entry->destination;
	curr_spd = cap_entry->owner;

//	printf("Invocation on cap %d from %x.\n", capability, 
//	       (unsigned int)curr_frame->current_composite_spd);
	/*
	 * If the spd that owns this capability is part of a composite
	 * spd that is the same as the composite spd that was the
	 * entry point for this composite spd.
	 *
	 * i.e. is the capability owner in the same protection domain
	 * (via ST) as the spd entry point to the protection domain.
	 */
	if (cap_entry->owner->composite_spd != curr_frame->current_composite_spd) {
		if (stale_il_or_error(curr_frame, cap_entry)) {
			printf("Error, incorrect capability (Cap %d has cspd %x, stk has %x).\n",
			       capability, (unsigned int)cap_entry->owner->composite_spd,
			       (unsigned int)curr_frame->current_composite_spd);
			print_stack(thd, curr_spd, dest_spd);
			/* FIXME: do something here like kill thread/spd */
		}
	}

	cap_entry->invocation_cnt++;

	if (cap_entry->il & IL_INV_UNMAP) {
		open_close_spd(&dest_spd->composite_spd->spd_info, 
			       &curr_spd->composite_spd->spd_info);
	} else {
		open_spd(&curr_spd->spd_info);
	}

	/* 
	 * ref count the composite spds:
	 * 
	 * FIXME, TODO: move composite pgd into each spd and ref count
	 * in spds.  Sum of all ref cnts is the composite ref count.
	 * This will eliminate the composite cache miss.
	 */
	
	/* add a new stack frame for the spd we are invoking (we're committed) */
	thd_invocation_push(thd, cap_entry->destination->composite_spd, sp, ip, usr_def);

//	printf("\tinvoking fn %x.\n", (unsigned int)cap_entry->dest_entry_instruction);

	return (fn_t)(cap_entry->dest_entry_instruction);
}

struct thd_invocation_frame *pop(struct thread *curr_thd)
{
	return thd_invocation_pop(curr_thd);
}
