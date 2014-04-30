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

typedef u16_t thdid_t;

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
} HALF_CACHE_ALIGNED;

/* TODO: replace with existing thread struct */
struct thread {
	thdid_t tid;
	int refcnt, invstk_top;
	struct comp_info comp_info; /* which scheduler to notify of events? FIXME: ignored for now */
	struct invstk_entry invstk[32];
	/* gp and fp registers */
};

struct cap_thd {
	struct cap_header h;
	struct thread *t;
	u32_t cpuid;
} __attribute__((packed));

static int 
thd_activate(struct captbl *t, capid_t cap, capid_t capin, struct thread *thd, capid_t compcap)
{
	struct cap_thd *tc;
	struct cap_comp *compc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, compcap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	tc = (struct cap_thd *)__cap_capactivate_pre(t, cap, capin, CAP_THD, &ret);
	if (!tc) return ret;

	/* initialize the thread */
	memcpy(&(thd->invstk[0].comp_info), &compc->info, sizeof(struct comp_info));
	thd->invstk[0].ip = thd->invstk[0].sp = 0;
	thd->tid          = 0; /* FIXME: need correct value */
	thd->refcnt       = 0;
	thd->invstk_top   = 0;

	/* initialize the capability */
	tc->t     = thd;
	tc->cpuid = 0; 		/* FIXME: add the proper call to get the cpuid */
	__cap_capactivate_post(&tc->h, CAP_THD, 0);

	return 0;
}

static int thd_deactivate(struct captbl *t, unsigned long cap, unsigned long capin)
{ return cap_capdeactivate(t, cap, capin, CAP_THD); }

void thd_init(void)
{ assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); }

#endif /* THREAD_H */
