/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef INV_H
#define INV_H

#ifdef LINUX_TEST
#include <stdio.h>
#endif

#include "component.h"
#include "thd.h"
#include "chal/call_convention.h"

struct cap_sinv {
	struct cap_header h;
	struct comp_info  comp_info;
	vaddr_t           entry_addr;
	invtoken_t        token;
} __attribute__((packed));

struct cap_sret {
	struct cap_header h;
	/* no other information needed */
} __attribute__((packed));

struct cap_asnd {
	struct cap_header h;
	cpuid_t           cpuid, arcv_cpuid;
	u32_t             arcv_capid, arcv_epoch; /* identify receiver */
	struct comp_info  comp_info;
} __attribute__((packed));

struct cap_arcv {
	struct cap_header h;
	struct comp_info  comp_info;
	u32_t             epoch;
	cpuid_t           cpuid;
	/* The thread to receive events, and the one to send events to (i.e. scheduler) */
	struct thread *thd, *event_notif;
	u8_t           depth;
} __attribute__((packed));

static int
sinv_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap, vaddr_t entry_addr, invtoken_t token)
{
	struct cap_sinv *sinvc;
	struct cap_comp *compc;
	int              ret;

	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	sinvc = (struct cap_sinv *)__cap_capactivate_pre(t, cap, capin, CAP_SINV, &ret);
	if (!sinvc) return ret;

	sinvc->token = token;

	memcpy(&sinvc->comp_info, &compc->info, sizeof(struct comp_info));
	sinvc->entry_addr = entry_addr;
	__cap_capactivate_post(&sinvc->h, CAP_SINV);

	return 0;
}

static int
sinv_deactivate(struct cap_captbl *t, capid_t capin, livenessid_t lid)
{
	return cap_capdeactivate(t, capin, CAP_SINV, lid);
}

static int
sret_activate(struct captbl *t, capid_t cap, capid_t capin)
{
	struct cap_sret *sretc;
	int              ret;

	sretc = (struct cap_sret *)__cap_capactivate_pre(t, cap, capin, CAP_SRET, &ret);
	if (!sretc) return ret;
	__cap_capactivate_post(&sretc->h, CAP_SRET);

	return 0;
}

static int
sret_deactivate(struct cap_captbl *t, capid_t capin, livenessid_t lid)
{
	return cap_capdeactivate(t, capin, CAP_SRET, lid);
}

static int
asnd_construct(struct cap_asnd *asndc, struct cap_arcv *arcvc, capid_t rcv_cap)
{
	/* FIXME: Add synchronization with __xx_pre and __xx_post */

	/* copy data from the arcv capability */
	memcpy(&asndc->comp_info, &arcvc->comp_info, sizeof(struct comp_info));
	asndc->h.type     = CAP_ASND;
	asndc->arcv_epoch = arcvc->epoch;
	asndc->arcv_cpuid = arcvc->cpuid;
	/* ...and initialize our own data */
	asndc->cpuid          = get_cpuid();
	asndc->arcv_capid     = rcv_cap;

	return 0;
}

static int
asnd_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t rcv_captbl, capid_t rcv_cap)
{
	struct cap_captbl *rcv_ct;
	struct cap_asnd *  asndc;
	struct cap_arcv *  arcvc;
	int                ret;

	rcv_ct = (struct cap_captbl *)captbl_lkup(t, rcv_captbl);
	if (unlikely(!rcv_ct || rcv_ct->h.type != CAP_CAPTBL)) return -EINVAL;

	arcvc = (struct cap_arcv *)captbl_lkup(rcv_ct->captbl, rcv_cap);
	if (unlikely(!arcvc || arcvc->h.type != CAP_ARCV)) return -EINVAL;

	asndc = (struct cap_asnd *)__cap_capactivate_pre(t, cap, capin, CAP_ASND, &ret);
	if (!asndc) return ret;

	ret = asnd_construct(asndc, arcvc, rcv_cap);
	__cap_capactivate_post(&asndc->h, CAP_ASND);

	return ret;
}

static int
asnd_deactivate(struct cap_captbl *t, capid_t capin, livenessid_t lid)
{
	return cap_capdeactivate(t, capin, CAP_ASND, lid);
}

int cap_ipi_process(struct pt_regs *regs);

/* send to a receive end-point within an interrupt */
int cap_hw_asnd(struct cap_asnd *asnd, struct pt_regs *regs);

static void
__arcv_setup(struct cap_arcv *arcv, struct thread *thd, struct tcap *tcap, struct thread *notif)
{
	assert(arcv && thd && tcap && !thd_bound2rcvcap(thd));
	arcv->thd                    = thd;
	thd->rcvcap.rcvcap_thd_notif = notif;
	thd_scheduler_set(thd, notif);
	if (notif) thd_rcvcap_take(notif);
	thd->rcvcap.isbound = 1;

	thd->rcvcap.rcvcap_tcap = tcap;
	tcap_ref_take(tcap);
	tcap_promote(tcap, thd);
}

static int
__arcv_teardown(struct cap_arcv *arcv, struct thread *thd)
{
	struct thread *notif;
	struct tcap *  tcap;

	tcap = thd->rcvcap.rcvcap_tcap;
	assert(tcap);
	if (tcap_ref(tcap) > 1 && tcap->arcv_ep == thd) return -1;

	notif = thd->rcvcap.rcvcap_thd_notif;
	if (notif) thd_rcvcap_release(notif);
	thd->rcvcap.isbound = 0;

	thd->rcvcap.rcvcap_tcap = NULL;
	tcap_ref_release(tcap);
	tcap->arcv_ep = NULL;

	return 0;
}

static struct thread *
arcv_thd_notif(struct thread *arcvt)
{
	return arcvt->rcvcap.rcvcap_thd_notif;
}

static int
arcv_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t comp_cap, capid_t thd_cap, capid_t tcap_cap,
              capid_t arcv_cap, int init)
{
	struct cap_comp *compc;
	struct cap_thd * thdc;
	struct cap_tcap *tcapc;
	struct cap_arcv *arcv_p, *arcvc; /* parent and new capability */
	struct thread *  thd;
	u8_t             depth = 0;
	int              ret;

	/* Find the constituent capability structures */
	compc = (struct cap_comp *)captbl_lkup(t, comp_cap);
	if (unlikely(!CAP_TYPECHK(compc, CAP_COMP))) return -EINVAL;

	thdc = (struct cap_thd *)captbl_lkup(t, thd_cap);
	if (unlikely(!CAP_TYPECHK_CORE(thdc, CAP_THD))) return -EINVAL;
	thd = thdc->t;

	tcapc = (struct cap_tcap *)captbl_lkup(t, tcap_cap);
	if (unlikely(!CAP_TYPECHK_CORE(tcapc, CAP_TCAP))) return -EINVAL;
	/* a single thread cannot be bound to multiple rcvcaps */
	if (thd_bound2rcvcap(thd)) return -EINVAL;
	assert(!thd->rcvcap.rcvcap_tcap); /* an unbound thread should not have a tcap */

	if (!init) {
		arcv_p = (struct cap_arcv *)captbl_lkup(t, arcv_cap);
		if (unlikely(!CAP_TYPECHK_CORE(arcv_p, CAP_ARCV))) return -EINVAL;

		depth = arcv_p->depth + 1;
		if (depth >= ARCV_NOTIF_DEPTH) return -EINVAL;
	}

	arcvc = (struct cap_arcv *)__cap_capactivate_pre(t, cap, capin, CAP_ARCV, &ret);
	if (!arcvc) return ret;

	memcpy(&arcvc->comp_info, &compc->info, sizeof(struct comp_info));

	arcvc->epoch = 0; /* FIXME: get the real epoch */
	arcvc->cpuid = get_cpuid();
	arcvc->depth = depth;

	__arcv_setup(arcvc, thd, tcapc->tcap, init ? thd : arcv_p->thd);

	__cap_capactivate_post(&arcvc->h, CAP_ARCV);

	return 0;
}

static int
arcv_deactivate(struct cap_captbl *t, capid_t capin, livenessid_t lid)
{
	struct cap_arcv *arcvc;

	arcvc = (struct cap_arcv *)captbl_lkup(t->captbl, capin);
	if (unlikely(!arcvc || arcvc->h.type != CAP_ARCV || arcvc->cpuid != get_cpuid())) return -EINVAL;
	if (thd_rcvcap_isreferenced(arcvc->thd)) return -EBUSY;
	if (__arcv_teardown(arcvc, arcvc->thd)) return -EBUSY;

	return cap_capdeactivate(t, capin, CAP_ARCV, lid);
}

static inline int
arcv_introspect(struct cap_arcv *r, unsigned long op, unsigned long *retval)
{
	switch (op) {
	case ARCV_GET_CPUID:
		*retval = r->cpuid;
		break;
	case ARCV_GET_THDID:
		*retval = r->thd->tid;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

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
sinv_call(struct thread *thd, struct cap_sinv *sinvc, struct pt_regs *regs, struct cos_cpu_local_info *cos_info)
{
	unsigned long ip, sp;

	ip = __userregs_getip(regs);
	sp = __userregs_getsp(regs);

	/*
	 * Note that we want this liveness lookup to proceed in
	 * parallel to the thread operations so we avoid stores in
	 * this path (to avoid serialization in the store buffer), and
	 * optimize the static branch prediction.
	 */
	if (unlikely(!ltbl_isalive(&(sinvc->comp_info.liveness)))) {
		printk("cos: sinv comp (liveness %d) doesn't exist!\n", sinvc->comp_info.liveness.id);
		// FIXME: add fault handling here.
		__userregs_set(regs, -EFAULT, __userregs_getsp(regs), __userregs_getip(regs));
		return;
	}

	if (unlikely(thd_invstk_push(thd, &sinvc->comp_info, ip, sp, cos_info))) {
		__userregs_set(regs, -1, sp, ip);
		return;
	}

	pgtbl_update(&sinvc->comp_info.pgtblinfo);
	chal_protdom_write(sinvc->comp_info.protdom);

	/* TODO: test this before pgtbl update...pre- vs. post-serialization */
	__userregs_sinvupdate(regs);
	__userregs_setinv(regs, thd->tid | (get_cpuid() << 16), sinvc->token,
			  sinvc->entry_addr);
	
	return;
}

static inline void
sret_ret(struct thread *thd, struct pt_regs *regs, struct cos_cpu_local_info *cos_info)
{
	struct comp_info *ci;
	unsigned long     ip, sp;
	prot_domain_t     protdom;

	ci = thd_invstk_pop(thd, &ip, &sp, &protdom, cos_info);
	if (unlikely(!ci)) {
		__userregs_set(regs, 0xDEADDEAD, 0, 0);
		return;
	}

	if (unlikely(!ltbl_isalive(&ci->liveness))) {
		printk("cos: ret comp (liveness %d) doesn't exist!\n", ci->liveness.id);
		// FIXME: add fault handling here.
		__userregs_set(regs, -EFAULT, __userregs_getsp(regs), __userregs_getip(regs));
		return;
	}

	pgtbl_update(&ci->pgtblinfo);
	chal_protdom_write(protdom);

	/* Set return sp and ip and function return value in eax */
	__userregs_set(regs, __userregs_getinvret(regs), sp, ip);
}

static void
inv_init(void)
{
//#define __OUTPUT_CAP_SIZE
#ifdef __OUTPUT_CAP_SIZE
	printk(" Cap header size %d, minimal cap %d\n SINV %d, SRET %d, ASND %d, ARCV %d\n CAP_COMP %d, CAP_THD %d, "
	       "CAP_CAPTBL %d, CAP_PGTBL %d\n",
	       sizeof(struct cap_header), sizeof(struct cap_min), sizeof(struct cap_sinv), sizeof(struct cap_sret),
	       sizeof(struct cap_asnd), sizeof(struct cap_arcv), sizeof(struct cap_comp), sizeof(struct cap_thd),
	       sizeof(struct cap_captbl), sizeof(struct cap_pgtbl));
#endif
	assert(sizeof(struct cap_sinv) <= __captbl_cap2bytes(CAP_SINV));
	assert(sizeof(struct cap_sret) <= __captbl_cap2bytes(CAP_SRET));
	assert(sizeof(struct cap_asnd) <= __captbl_cap2bytes(CAP_ASND));
	assert(sizeof(struct cap_arcv) <= __captbl_cap2bytes(CAP_ARCV));
}

#endif /* INV_H */
