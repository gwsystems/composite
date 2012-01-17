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

/* FIXME: should have a set of saved fault regs per thread. */
int regs_active = 0; 
struct cos_regs regs;

int fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	if (regs_active) BUG();
	regs_active = 1;
	cos_regs_save(cos_get_thd_id(), spdid, fault_addr, &regs);
	printc("Thread %d faults in spd %d @ %p\n", 
	       cos_get_thd_id(), spdid, fault_addr);
	cos_regs_print(&regs);
	BUG(); 			/* no fault is a good fault currently */
//	sched_block(spdid, 0);
	return 0;
}
