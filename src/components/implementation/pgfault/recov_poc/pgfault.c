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
#include <sched.h>
#include <print.h>
#include <fault_regs.h>

/* FIXME: should have a set of saved fault regs per thread. */
int regs_active = 0; 
struct cos_regs regs;

int fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long r_ip; 	/* the ip to return to */
	int tid = cos_get_thd_id();

	if (regs_active) BUG();
	regs_active = 1;
	cos_regs_save(tid, spdid, fault_addr, &regs);
	printc("Thread %d faults in spd %d @ %p\n", 
	       tid, spdid, fault_addr);
	cos_regs_print(&regs);

	/* remove from the invocation stack the faulting component! */
	cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0);
	
	/* Manipulate the return address of the component that called
	 * the faulting component... */
	r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 2, 0);
	/* ...and set it to its value -8, which is the fault handler
	 * of the stub. */
	cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, r_ip-8, 2);

	return 0;
}
