/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef INV_H
#define INV_H

#include "component.h"
#include "thd.h"
#include "call_convention.h"

/* Note: h.poly is the u16_t that is passed up to the component as spdid (in the current code) */
struct cap_sinv {
	struct cap_header h;
	struct comp_info comp_info;
	vaddr_t entry_addr;
} __attribute__((packed));

struct cap_sret {
	struct cap_header h;
	/* no other information needed */
} __attribute__((packed));

struct cap_asnd {
	struct cap_header h;
	u32_t cpuid;
	u32_t arcv_cpuid, arcv_capid, arcv_epoch; /* identify reciever */
	struct comp_info comp_info;

	/* deferrable server to rate-limit IPIs */
	u32_t budget, period, replenish_amnt;
	u64_t replenish_time; 	   /* time of last replenishment */
} __attribute__((packed));

struct cap_arcv {
	struct cap_header h;
	struct comp_info comp_info;
	u32_t pending, cpuid, epoch;
	u32_t thd_epoch;
	struct thread *thd;
} __attribute__((packed));

static int 
sinv_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap, vaddr_t entry_addr)
{
	struct cap_sinv *sinvc;
	struct cap_comp *compc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;
	
	sinvc = (struct cap_sinv *)__cap_capactivate_pre(t, cap, capin, CAP_SINV, &ret);
	if (!sinvc) return ret;
	memcpy(&sinvc->comp_info, &compc->info, sizeof(struct comp_info));
	sinvc->entry_addr = entry_addr;
	__cap_capactivate_post(&sinvc->h, CAP_SINV, compc->h.poly);

	return 0;
}

static int sinv_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_SINV); }

static int 
sret_activate(struct captbl *t, capid_t cap, capid_t capin)
{
	struct cap_sret *sretc;
	int ret;

	sretc = (struct cap_sret *)__cap_capactivate_pre(t, cap, capin, CAP_SRET, &ret);
	if (!sretc) return ret;
	__cap_capactivate_post(&sretc->h, CAP_SRET, 0);

	return 0;
}

static int sret_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_SRET); }

static int
asnd_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t rcv_captbl, capid_t rcv_cap, u32_t budget, u32_t period)
{
	struct cap_captbl *rcv_ct;
	struct cap_asnd *asndc;
	struct cap_arcv *arcvc;
	int ret;

	rcv_ct = (struct cap_captbl *)captbl_lkup(t, rcv_captbl);
	if (unlikely(!rcv_ct || rcv_ct->h.type != CAP_CAPTBL)) return -EINVAL;

	arcvc = (struct cap_arcv *)captbl_lkup(rcv_ct->captbl, rcv_cap);
	if (unlikely(!arcvc || arcvc->h.type != CAP_ARCV)) return -EINVAL;
	
	asndc = (struct cap_asnd *)__cap_capactivate_pre(t, cap, capin, CAP_ASND, &ret);
	if (!asndc) return ret;
	memcpy(&asndc->comp_info, &arcvc->comp_info, sizeof(struct comp_info));
	asndc->arcv_epoch     = arcvc->epoch;
	asndc->arcv_cpuid     = arcvc->cpuid;
	asndc->arcv_capid     = rcv_cap;
	asndc->period         = period;
	asndc->budget         = budget;
	asndc->replenish_amnt = budget;
	//FIXME:  add rdtscll(asndc->replenish_time);
	__cap_capactivate_post(&asndc->h, CAP_ASND, 0);

	return 0;
}

static int asnd_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_ASND); }

static int
arcv_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap, capid_t thd_cap)
{
	struct cap_comp *compc;
	struct cap_thd  *thdc;
	struct cap_arcv *arcvc;
	int ret;

	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	thdc = (struct cap_thd *)captbl_lkup(t, thd_cap);
	if (unlikely(!thdc || thdc->h.type != CAP_THD)) return -EINVAL;
	/* if (thdc->cpuid != get_cpuid()) return -EINVAL; */

	arcvc = (struct cap_arcv *)__cap_capactivate_pre(t, cap, capin, CAP_ARCV, &ret);
	if (!arcvc) return ret;

	memcpy(&arcvc->comp_info, &compc->info, sizeof(struct comp_info));

	arcvc->pending   = 0;
	arcvc->cpuid     = thdc->t->cpuid;
	arcvc->epoch     = 0; 	  /* FIXME: get the real epoch */
	arcvc->thd       = thdc->t; /* FIXME: do reference counting for the thread here */
	arcvc->thd_epoch = 0; 	  /* FIXME: get the real epoch */
	__cap_capactivate_post(&arcvc->h, CAP_ARCV, 0);
	
	return 0;
}

static int arcv_deactivate(struct captbl *t, capid_t cap, capid_t capin)
{ return cap_capdeactivate(t, cap, capin, CAP_ARCV); }

/* 
 * Invocation (call and return) fast path.  We want this to be as
 * optimized as possible.  The only two optimizations not yet
 * performed are 1) to cache the invocation stack pointer with the
 * thread id to avoid that cache-line access, and 2) to cache the
 * entire invocation stack on the kernel stack.  Option 1. represents
 * a more practical amount of caching.  Both require consistency
 * between the thread structure and the cached contents to be achieved
 * on context switches.
 */

static inline void
sinv_call(struct thread *thd, struct cap_sinv *sinvc, struct pt_regs *regs)
{
	unsigned long ip, sp;

	ip = __userregs_getip(regs);
	sp = __userregs_getsp(regs);
	/* FIXME: check liveness */
	if (unlikely(thd_invstk_push(thd, &sinvc->comp_info, ip, sp))) {
		__userregs_set(regs, -1, sp, ip);
		return;
	}

	pgtbl_update(sinvc->comp_info.pgtbl);

	__userregs_sinvupdate(regs);
	__userregs_set(regs, thd->tid | (get_cpuid_fast() << 16),
		       sinvc->h.poly /* calling component id */, sinvc->entry_addr);
}

static inline void
sret_ret(struct thread *thd, struct pt_regs *regs)
{
	struct comp_info *ci;
	unsigned long ip, sp;

	ci = thd_invstk_pop(thd, &ip, &sp);
	if (unlikely(!ci)) {
		__userregs_set(regs, 0xDEADDEAD, 0, 0);
		return;
	}
	/* FIXME: check liveness */
	pgtbl_update(ci->pgtbl);
	__userregs_set(regs, __userregs_getinvret(regs), sp, ip);
}

static void inv_init(void)
{ 
	assert(sizeof(struct cap_sinv) <= __captbl_cap2bytes(CAP_SINV)); 
	assert(sizeof(struct cap_asnd) <= __captbl_cap2bytes(CAP_ASND)); 
	assert(sizeof(struct cap_arcv) <= __captbl_cap2bytes(CAP_ARCV)); 
}

#endif /* INV_H */
