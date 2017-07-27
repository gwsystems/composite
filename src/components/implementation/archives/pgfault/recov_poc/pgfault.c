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

#include <failure_notif.h>

/* FIXME: should have a set of saved fault regs per thread. */
int regs_active = 0; 
struct cos_regs regs;

int fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long r_ip; 	/* the ip to return to */
	int tid = cos_get_thd_id();

	/* START UNCOMMENT FOR FAULT INFO */
#if 0
	int i;
	if (regs_active) BUG();
	regs_active = 1;
	cos_regs_save(tid, spdid, fault_addr, &regs);
	printc("Thread %d faults in spd %d @ %p\n",
	       tid, spdid, fault_addr);
	cos_regs_print(&regs);
	regs_active = 0;

	for (i = 0 ; i < 5 ; i++) 
		printc("Frame ip:%lx, sp:%lx\n",
		       cos_thd_cntl(COS_THD_INVFRM_IP, tid, i, 0),
		       cos_thd_cntl(COS_THD_INVFRM_SP, tid, i, 0));
#endif
	/* END UNCOMMENT FOR FAULT INFO */

	/* remove from the invocation stack the faulting component! */
	assert(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0));

	/* Manipulate the return address of the component that called
	 * the faulting component... */
	assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
	/* ...and set it to its value -8, which is the fault handler
	 * of the stub. */
	assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));

#if 0
	for (i = 0 ; i < 5 ; i++) 
		printc ("after adjusment:\n");
		printc("Frame ip:%lx, sp:%lx\n",  
		       cos_thd_cntl(COS_THD_INVFRM_IP, tid, i, 0),
		       cos_thd_cntl(COS_THD_INVFRM_SP, tid, i, 0));
#endif

	/* 
	 * Look at the booter: when recover is happening, the sstub is
	 * set to 0x1, thus we should just wait till recovery is done.
	 */
	if ((int)ip == 1) failure_notif_wait(cos_spd_id(), spdid);
	else         failure_notif_fail(cos_spd_id(), spdid);

	return COS_FLT_PGFLT;
}
