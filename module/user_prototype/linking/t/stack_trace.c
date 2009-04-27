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
 * FIXME: this will not yet work with preemption and currently does
 * not do memory aliasing, so it will only work when the component to
 * be traced is in the same protection domain.  This is a huge
 * limitation.
 */
static void walk_stack(unsigned short int tid, unsigned long *sp)
{
	/* tracking the frame pointers, and printing instruction pointers */
	unsigned long *fp, *ip;
	unsigned int status = cos_thd_cntl(COS_THD_STATUS, tid, 0, 0);

	if (status & 1/*THD_STATE_PREEMPTED*/) {
		printc("\tstatus: PREEMPTED\n");
		sp = (unsigned long *)cos_thd_cntl(COS_THD_INVFRM_FP, tid, 0, 0);
	} 
	fp = sp;
	ip = fp+1;

	while (fp != NULL) {

		if (fp < sp || sp+1024 < fp) {
			printc("\tinvalid frame pointer for thread %d (sp %p, fp %p, ip %p).\n", 
			       tid, sp, fp, ip);
			return;
		}
		printc("\t<%d: ip:%x>\n", tid, (unsigned int)*ip);

		fp = (unsigned long *)*fp;
		ip = fp+1;
	}
	return;
}

void st_trace_thd(unsigned short int tid)
{
	int i, ret = 1;

	for (i = 0 ; ret > 0 ; i++) {
		unsigned int ip, sp;
		ret = cos_thd_cntl(COS_THD_INV_FRAME, tid, i, 0);
		if (ret) {
			ip  = cos_thd_cntl(COS_THD_INVFRM_IP, tid, i, 0);
			sp  = cos_thd_cntl(COS_THD_INVFRM_SP, tid, i, 0);
			printc("st -- [%d: %d (ip:%x,sp:%x)]\n", tid, ret, ip, sp);
			walk_stack(tid, (unsigned long *)sp);
		}
	}

}
