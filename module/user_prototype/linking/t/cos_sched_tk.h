/**
 * Copyright 2009 by The George Washington University
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#ifndef   	COS_SCHED_TK_H
#define   	COS_SCHED_TK_H

struct sched_ops {
	/* add a new schedulable thread */
	int (*thread_new)(struct sched_thd *t);
	int (*thread_params_set)(struct sched_thd *t, char *params);
	/* add a new upcall, which is not currently runnable */
	int (*upcall_new)(struct sched_thd *t);
	int (*thread_remove)(struct sched_thd *t);
	/* Args include a thread and the amount of time it spent
	 * processing.  If t==NULL, then time has passed without being
	 * attributed to a thread.  This is either idle time, or if a
	 * child scheduler because the parent removed you from the
	 * CPU. */
	int (*time_elapsed)(struct sched_thd *t, u32_t processing_time);

	/* Which thread should we schedule next?  Return NULL if
	 * there are no currently executable threads.  Ensure that the
	 * chosen thread is not t.
	 */
	struct sched_thd *(*schedule)(struct sched_thd *t);
	int (*thread_block)(struct sched_thd *t);
	/* block t, and it is blocking with a dependency on thread dep */
	int (*thread_block_dependency)(struct sched_thd *t, struct sched_thd *dep);
	int (*thread_wakeup)(struct sched_thd *t);
};

#endif 	    /* !COS_SCHED_TK_H */
