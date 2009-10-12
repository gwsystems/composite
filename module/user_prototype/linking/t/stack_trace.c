/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

/* Access control? */

/* 
 * FIXME: this currently does not do memory aliasing, so it will only
 * work when the component to be traced is in the same protection
 * domain.  This is a huge limitation.
 */
static void walk_stack(unsigned short int tid, unsigned long *sp)
{
	/* tracking the frame pointers, and printing instruction pointers */
	unsigned long *fp, *ip;

	fp = sp;
	ip = fp+1;

	while (fp != NULL) {
		if (fp < sp || sp+1024 < fp) {
			printc("\tinvalid frame pointer for thread %d (sp %p, fp %p, ip %p).\n", 
			       tid, sp, fp, ip);
			return;
		}
		printc("\t\t<ip:%x>\n", (unsigned int)*ip);

		fp = (unsigned long *)*fp;
		ip = fp+1;
	}
	return;
}

void st_trace_thd(unsigned short int tid)
{
	int i, ret = 1;
	unsigned int status = cos_thd_cntl(COS_THD_STATUS, tid, 0, 0);
	unsigned long *sp = NULL;

	if (status & 1/*THD_STATE_PREEMPTED*/) {
		printc("\ttid %d status PREEMPTED:\n", tid);
		sp = (unsigned long *)cos_thd_cntl(COS_THD_INVFRM_FP, tid, 0, 0);
	} else {
		if (cos_get_thd_id() == tid) {
			__asm__ __volatile__("movl %%ebp, %0" : "=m" (sp));
		}
		printc("\ttid %d:\n", tid);
	}
	for (i = 0 ; ret > 0 ; i++) {
		unsigned int ip;
		ret = cos_thd_cntl(COS_THD_INV_FRAME, tid, i, 0);
		if (ret) {
			ip  = cos_thd_cntl(COS_THD_INVFRM_IP, tid, i, 0);
			if (0 != i || NULL == sp) {
				sp = (unsigned long *)cos_thd_cntl(COS_THD_INVFRM_SP, tid, i, 0);
			}
			printc("\t[spdid:%d, (ip:%x,sp:%p)]\n", ret, ip, sp);
			walk_stack(tid, sp);
		}
	}

}
