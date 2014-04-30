/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef THREAD_H
#define THREAD_H

#include "component.h"
#include <cap_ops.h>

#define THD_INVSTK_MAXSZ 32

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
} HALF_CACHE_ALIGNED;

/* TODO: replace with existing thread struct */
struct thread {
	thdid_t tid;
	int refcnt, invstk_top;
	struct comp_info comp_info; /* which scheduler to notify of events? FIXME: ignored for now */
	struct invstk_entry invstk[THD_INVSTK_MAXSZ];
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

extern struct thread *__thd_current;
static inline struct thread *thd_current(void) 
{ return __thd_current; }

static inline void thd_current_update(struct thread *thd)
{ __thd_current = thd; }

static inline struct comp_info *
thd_invstk_current(struct thread *thd, unsigned long *ip, unsigned long *sp)
{
	struct invstk_entry *curr;

	/* 
	 * TODO: will be worth caching the invocation stack top along
	 * with the current thread pointer to avoid the invstk_top
	 * cacheline access.
	 */
	curr = &thd->invstk[thd->invstk_top];
	*ip = curr->ip;
	*sp = curr->sp;
	return &curr->comp_info;
}

static inline int
thd_invstk_push(struct thread *thd, struct comp_info *ci, unsigned long ip, unsigned long sp)
{
	struct invstk_entry *top, *prev;

	prev = &thd->invstk[thd->invstk_top];
	top  = &thd->invstk[thd->invstk_top+1];
	if (unlikely(thd->invstk_top >= THD_INVSTK_MAXSZ)) return -1;
	thd->invstk_top++;
	prev->ip = ip;
	prev->sp = sp;
	memcpy(&top->comp_info, ci, sizeof(struct comp_info));
	top->ip  = top->sp = 0;

	return 0;
}

static inline struct comp_info *
thd_invstk_pop(struct thread *thd, unsigned long *ip, unsigned long *sp)
{
	if (unlikely(thd->invstk_top == 0)) return NULL;
	thd->invstk_top--;
	return thd_invstk_current(thd, ip, sp);
}

void thd_init(void)
{ assert(sizeof(struct cap_thd) <= __captbl_cap2bytes(CAP_THD)); }

#endif /* THREAD_H */
