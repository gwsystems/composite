/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef THREAD_H
#define THREAD_H

#include <component.h>
#include <cap_ops.h>

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
} HALF_CACHE_ALIGNED;

/* TODO: replace with existing thread struct */
struct thread {
	int invstk_top;
	struct invstk_entry invstk[32];
};

struct cap_thd {
	struct cap_header h;
	struct thread *t;
	u32_t cpuid;
} __attribute__((packed));

static int 
thd_activate(struct captbl *t, struct thread *thd, unsigned long cap, unsigned long capin)
{
	struct cap_thd *tc;
	int ret;

	tc = (struct cap_thd *)__cap_capactivate_pre(t, cap, capin, CAP_THD, &ret);
	if (!tc) return ret;
	tc->t     = thd;
	tc->cpuid = 0; 		/* FIXME: add the proper call to get the cpuid */
	__cap_capactivate_post(tc, CAP_THD, 0);
}

static int thd_deactivate(struct captbl *t, unsigned long cap, unsigned long capin)
{ return cap_capdeactivate(t, cap, capin, CAP_THD); }

void thd_init(void)
{ assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); }

#endif /* THREAD_H */
