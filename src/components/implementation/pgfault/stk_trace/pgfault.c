/**
 * Copyright 2011 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#include <cos_component.h>
#include <pgfault.h>
//#include <sched.h>
#include <print.h>
#include <fault_regs.h>
#include <stkmgr.h>

/* FIXME: should have a set of saved fault regs per thread. */
int regs_active = 0; 
struct cos_regs regs;

static unsigned long * 
map_stack(spdid_t spdid, vaddr_t extern_stk)
{
	static unsigned long *stack = 0;
	vaddr_t extern_addr;
	
	if (!stack) stack = cos_get_vas_page();
	extern_addr = round_to_page(extern_stk);

	if (stkmgr_stack_introspect(cos_spd_id(), (vaddr_t)stack, spdid, extern_addr)) BUG();
	
	return stack;
}

static void
unmap_stack(spdid_t spdid, unsigned long *stack)
{
	stkmgr_stack_close(spdid, (vaddr_t)stack);
}

/* 
 * first = 1 if we are to print out the first frame referenced by fp.
 */
static void 
walk_stack(spdid_t spdid, unsigned long *fp, unsigned long *stack) 
{
	unsigned long fp_off = 0, ip;

	do {
		unsigned long prev_off = fp_off;
		
		/* Since the virtual addresses aren't valid, we have
		 * to take an offset into the page */
		fp_off = ((unsigned long)fp & (~PAGE_MASK))/sizeof(unsigned long);
		if (prev_off >= fp_off) break;
		
		fp = stack + fp_off;
		ip = *(fp+1);
		fp = (unsigned long *)*fp;

		/* -5 as "call <fn>" is a 5 byte instruction (with argument) */
		printc("\t[%d, %lx]\n", spdid, ip-5);
	} while (fp);
}

static void 
walk_stack_all(spdid_t spdid, struct cos_regs *regs)
{
	unsigned long *fp, *stack, fp_off;
	int i, tid = cos_get_thd_id();

	printc("Stack trace for thread %d [spdid, instruction pointer]:\n", tid);

	fp = (unsigned long *)regs->regs.bp;
	stack = map_stack(spdid, (vaddr_t)fp);
	printc("\t[%d, %lx]\n", spdid, (unsigned long)regs->regs.ip);
	walk_stack(spdid, fp, stack);
	unmap_stack(spdid, stack);

	assert(cos_spd_id() == cos_thd_cntl(COS_THD_INV_FRAME, tid, 0, 0));
	assert(spdid == cos_thd_cntl(COS_THD_INV_FRAME, tid, 1, 0));

	for (i = 2 ; (spdid = cos_thd_cntl(COS_THD_INV_FRAME, tid, i, 0)) != 0 ; i++) {
		unsigned long sp;

		/* We're ignoring the initial IPs the IP is in the
		 * invocation stubs, and noone cares about the
		 * stubs */
		sp = cos_thd_cntl(COS_THD_INVFRM_SP, tid, i, 0);
		assert(sp);
		
		stack = map_stack(spdid, sp);
		/* The invocation stubs save ebp last, thus *(esp+16)
		 * = ebp.  This offset corresponds to the number of
		 * registers pushed in
		 * SS_ipc_client_marshal_args... */
		fp_off = ((sp & (~PAGE_MASK))/sizeof(unsigned long)) + 4;
		fp = (unsigned long *)&stack[fp_off];
		
		walk_stack(spdid, fp, stack);
		unmap_stack(spdid, stack);
	}
	
}

void fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *orig_ip)
{
	if (regs_active) BUG();
	regs_active = 1;
	cos_regs_save(cos_get_thd_id(), spdid, fault_addr, &regs);
	printc("Thread %d faults in spd %d @ %p\n", 
	       cos_get_thd_id(), spdid, fault_addr);
	cos_regs_print(&regs);

	walk_stack_all(spdid, &regs);
	
	/* No fault is a good fault currently.  Thus if we get _any_
	 * fault, we bomb out here.  Look into the stack-trace, as
	 * that is where the problem is. */
	BUG(); 			

	return;
}
