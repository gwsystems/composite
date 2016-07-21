/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/thd.h"
#include "include/chal/call_convention.h"
#include "include/ipi_cap.h"
#include "include/liveness_tbl.h"
#include "include/chal/cpuid.h"
#include "include/tcap.h"
#include "include/chal/defs.h"
#include "include/hw.h"

#define COS_DEFAULT_RET_CAP 0

/*
 * TODO: switch to a dedicated TLB flush thread (in a separate
 * protection domain) to do this.
 */
void
tlb_mandatory_flush(void *arg)
{
	unsigned long long t;
	(void)arg;

	rdtscll(t);
	/* Order is important: get tsc before action. */
	chal_flush_tlb();
	/* But commit after. */
	tlb_quiescence[get_cpuid()].last_mandatory_flush = t;
}

#define MAX_LEN 512
extern char timer_detector[PAGE_SIZE] PAGE_ALIGNED;
static inline int
printfn(struct pt_regs *regs)
{
	char *str;
	int len;
	char kern_buf[MAX_LEN];

	str     = (char *)__userregs_get1(regs);
	len     = __userregs_get2(regs);

	if (len < 1) goto done;
	if (len >= MAX_LEN) len = MAX_LEN - 1;
	memcpy(kern_buf, str, len);

	/*
	 * Hack of all hacks.  Manually request TLB flushes from
	 * user-level.  Should be replaced with invocations to async
	 * invocations instead.
	 */
	if (len >= 7) {
		if (kern_buf[0] == 'F' && kern_buf[1] == 'L' && kern_buf[2] == 'U' &&
		    kern_buf[3] == 'S' && kern_buf[4] == 'H' && kern_buf[5] == '!') {
			int target_cpu = kern_buf[6];

			if (target_cpu == get_cpuid()) {
				tlb_mandatory_flush(NULL);
			} else {
				/* FIXME: avoid using this band-aid. */
				chal_remote_tlb_flush(target_cpu);
			}

			__userregs_set(regs, 0,
				       __userregs_getsp(regs), __userregs_getip(regs));

			return 0;
		}
	}

	kern_buf[len] = '\0';
	printk("%s", kern_buf);
done:
	__userregs_set(regs, 0, __userregs_getsp(regs), __userregs_getip(regs));

	return 0;
}

void cos_cap_ipi_handling(void);
void
cos_cap_ipi_handling(void)
{
	int idx, end;
	struct IPI_receiving_rings *receiver_rings;
	struct xcore_ring *ring;

	receiver_rings = &IPI_cap_dest[get_cpuid()];

	/* We need to scan the entire buffer once. */
	idx = receiver_rings->start;
	end = receiver_rings->start - 1; //end is int type. could be -1.
	receiver_rings->start = (receiver_rings->start + 1) % NUM_CPU;

	/* scan the first half */
	for (; idx < NUM_CPU; idx++) {
		ring = &receiver_rings->IPI_source[idx];
		if (ring->sender != ring->receiver) {
			process_ring(ring);
		}
	}

	/* and scan the second half */
	for (idx = 0; idx <= end; idx++) {
		ring = &receiver_rings->IPI_source[idx];
		if (ring->sender != ring->receiver) {
			process_ring(ring);
		}
	}

	return;
}

static void
kmem_unalloc(unsigned long *pte)
{
	/*
	 * Release the kmem page. We must be the only one has access
	 * to it.  It is most useful when newly allocated kernel
	 * memory must be quickly de-allocated due to an error
	 * elsewhere.
	 */
	unsigned long old = *pte;

	assert(old & PGTBL_COSKMEM);
	retypetbl_deref((void *)(old & PGTBL_FRAME_MASK));
	*pte = old & ~PGTBL_COSKMEM;
}

/*
 * The deact_pre / _post are used by kernel object deactivation:
 * cap_captbl, cap_pgtbl and thd.
 */
int
kmem_deact_pre(struct cap_header *ch, struct captbl *ct, capid_t pgtbl_cap,
	       capid_t cosframe_addr, unsigned long **p_pte, unsigned long *v)
{
	struct cap_pgtbl *cap_pt;
	u32_t flags, old_v, pa;
	u64_t curr;
	int ret;

	assert(ct && ch);
	if (!pgtbl_cap || !cosframe_addr) cos_throw(err, -EINVAL);

	cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, pgtbl_cap);
	if (cap_pt->h.type != CAP_PGTBL) cos_throw(err, -EINVAL);

	/* get the pte to the cos frame. */
	*p_pte = pgtbl_lkup_pte(cap_pt->pgtbl, cosframe_addr, &flags);
	if (!p_pte) cos_throw(err, -EINVAL);
	old_v = *v = **p_pte;

	pa = old_v & PGTBL_FRAME_MASK;
	if (!(old_v & PGTBL_COSKMEM)) cos_throw(err, -EINVAL);
	assert(!(old_v & PGTBL_QUIESCENCE));

	/* Scan the page to make sure there's nothing left. */
	if (ch->type == CAP_CAPTBL) {
		struct cap_captbl *deact_cap = (struct cap_captbl *)ch;
		void *page = deact_cap->captbl;
		u32_t l = deact_cap->refcnt_flags;

		if (chal_pa2va((paddr_t)pa) != page) cos_throw(err, -EINVAL);

		/* Require freeze memory and wait for quiescence
		 * first! */
		if (!(l & CAP_MEM_FROZEN_FLAG)) {
			cos_throw(err, -EQUIESCENCE);
		}
		/* Quiescence check! */
		if (deact_cap->lvl == 0) {
			/* top level has tlb quiescence period (due to
			 * the optimization to avoid current_component
			 * lookup on invocation path). */
			if (!tlb_quiescence_check(deact_cap->frozen_ts)) return -EQUIESCENCE;
		} else {
			/* other levels have kernel quiescence period. */
			rdtscll(curr);
			if (!QUIESCENCE_CHECK(curr, deact_cap->frozen_ts, KERN_QUIESCENCE_CYCLES)) return -EQUIESCENCE;
		}

		/* set the scan flag to avoid concurrent scanning. */
		if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, l | CAP_MEM_SCAN_FLAG) != CAS_SUCCESS) return -ECASFAIL;

		/**************************************************/
		/* When gets here, we know quiescence has passed. and
		 * we are holding the scan lock. */
		/**************************************************/
		if (deact_cap->lvl < CAPTBL_DEPTH - 1) {
			ret = kmem_page_scan(page, PAGE_SIZE);
		} else {
			ret = captbl_kmem_scan(deact_cap);
		}

		if (ret) {
			/* unset scan and frozen bits. */
			cos_cas((unsigned long *)&deact_cap->refcnt_flags, l | CAP_MEM_SCAN_FLAG,
				l & ~(CAP_MEM_FROZEN_FLAG | CAP_MEM_SCAN_FLAG));
			cos_throw(err, ret);
		}
		cos_cas((unsigned long *)&deact_cap->refcnt_flags, l | CAP_MEM_SCAN_FLAG, l);
	} else if (ch->type == CAP_PGTBL) {
		struct cap_pgtbl *deact_cap = (struct cap_pgtbl *)ch;
		void *page = deact_cap->pgtbl;
		u32_t l = deact_cap->refcnt_flags;

		if (chal_pa2va((paddr_t)pa) != page) cos_throw(err, -EINVAL);

		/* Require freeze memory and wait for quiescence
		 * first! */
		if (!(l & CAP_MEM_FROZEN_FLAG)) {
			cos_throw(err, -EQUIESCENCE);
		}

		/* Quiescence check! */
		if (deact_cap->lvl == 0) {
			/* top level has tlb quiescence period. */
			if (!tlb_quiescence_check(deact_cap->frozen_ts)) return -EQUIESCENCE;
		} else {
			/* other levels have kernel quiescence
			 * period. (but the mapping scan will ensure
			 * tlb quiescence implicitly). */
			rdtscll(curr);
			if (!QUIESCENCE_CHECK(curr, deact_cap->frozen_ts, KERN_QUIESCENCE_CYCLES)) return -EQUIESCENCE;
		}

		/* set the scan flag to avoid concurrent scanning. */
		if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, l | CAP_MEM_SCAN_FLAG) != CAS_SUCCESS) return -ECASFAIL;

		if (deact_cap->lvl == 0) {
			/* PGD: only scan user mapping. */
			ret = kmem_page_scan(page, PAGE_SIZE - KERNEL_PGD_REGION_SIZE);
		} else if (deact_cap->lvl == PGTBL_DEPTH - 1) {
			/* Leaf level, scan mapping. */
			ret = pgtbl_mapping_scan(deact_cap);
		} else {
			/* don't have this with 2-level pgtbl. */
			ret = kmem_page_scan(page, PAGE_SIZE);
		}

		if (ret) {
			/* unset scan and frozen bits. */
			cos_cas((unsigned long *)&deact_cap->refcnt_flags, l | CAP_MEM_SCAN_FLAG,
				l & ~(CAP_MEM_FROZEN_FLAG | CAP_MEM_SCAN_FLAG));
			cos_throw(err, ret);
		}
		cos_cas((unsigned long *)&deact_cap->refcnt_flags, l | CAP_MEM_SCAN_FLAG, l);
	} else {
		/* currently only captbl and pgtbl pages need to be
		 * scanned before deactivation. */
		struct cap_thd *tc = (struct cap_thd *)ch;

		if (chal_pa2va((paddr_t)pa) != (void *)(tc->t)) cos_throw(err, -EINVAL);

		assert(ch->type == CAP_THD);
	}

	return 0;
err:

	return ret;
}

/* Updates the pte, deref the frame and zero out the page. */
int
kmem_deact_post(unsigned long *pte, unsigned long old_v)
{
	int ret;
	u32_t new_v;

	/* Unset coskmem bit. Release the kmem frame. */
	new_v = old_v & (~PGTBL_COSKMEM);
	if (cos_cas(pte, old_v, new_v) != CAS_SUCCESS) cos_throw(err, -ECASFAIL);

	ret = retypetbl_deref((void *)(old_v & PGTBL_FRAME_MASK));
	if (ret) {
		/* FIXME: handle this case? */
		cos_cas(pte, new_v, old_v);
		cos_throw(err, ret);
	}
	/* zero out the page to avoid info leaking. */
	memset(chal_pa2va((paddr_t)(old_v & PGTBL_FRAME_MASK)), 0, PAGE_SIZE);

	return 0;
err:
	return ret;
}

/*
 * Copy a capability from a location in one captbl/pgtbl to a location
 * in the other.  Fundamental operation used to delegate capabilities.
 * TODO: should limit the types of capabilities this works on.
 */
static inline int
cap_cpy(struct captbl *t, capid_t cap_to, capid_t capin_to,
	capid_t cap_from, capid_t capin_from)
{
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
	struct cap_header *ctto, *ctfrom;
	int sz, ret;
	cap_t cap_type;

	ctfrom = captbl_lkup(t, cap_from);
	if (unlikely(!ctfrom)) return -ENOENT;
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);

	cap_type = ctfrom->type;

	if (cap_type == CAP_CAPTBL) {
		u32_t old_v, l;
		cap_t type;

		ctfrom = captbl_lkup(((struct cap_captbl *)ctfrom)->captbl, capin_from);
		if (unlikely(!ctfrom)) return -ENOENT;

		type = ctfrom->type;
		sz   = __captbl_cap2bytes(type);

		ctto = __cap_capactivate_pre(t, cap_to, capin_to, type, &ret);
		if (!ctto) return -EINVAL;

		memcpy(ctto->post, ctfrom->post, sz - sizeof(struct cap_header));

		if (type == CAP_THD) {
			/* thd is per-core. refcnt in thd struct. */
			struct thread *thd = ((struct cap_thd *)ctfrom)->t;

			thd->refcnt++;
		} else if (type == CAP_CAPTBL) {
			struct cap_captbl *parent = (struct cap_captbl *)ctfrom;
			struct cap_captbl *child  = (struct cap_captbl *)ctto;

			old_v = l = parent->refcnt_flags;
			if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
			if ((l & CAP_REFCNT_MAX) == CAP_REFCNT_MAX) return -EOVERFLOW;

			cos_cas((unsigned long *)&(parent->refcnt_flags), old_v, l + 1);

			child->refcnt_flags = 1;
			child->parent = parent;
		} else if (type == CAP_PGTBL) {
			struct cap_pgtbl *parent = (struct cap_pgtbl *)ctfrom;
			struct cap_pgtbl *child  = (struct cap_pgtbl *)ctto;

			old_v = l = parent->refcnt_flags;
			if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
			if ((l & CAP_REFCNT_MAX) == CAP_REFCNT_MAX) return -EOVERFLOW;

			cos_cas((unsigned long *)&(parent->refcnt_flags), old_v, l + 1);

			child->refcnt_flags = 1;
			child->parent = parent;
		}
		__cap_capactivate_post(ctto, type);
	} else if (cap_type == CAP_PGTBL) {
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
		unsigned long *f, old_v;
		u32_t flags;

		ctto = captbl_lkup(t, cap_to);
		if (unlikely(!ctto)) return -ENOENT;
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
		if (unlikely(ctto->type != cap_type)) return -EINVAL;
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
		if (unlikely(((struct cap_pgtbl *)ctto)->refcnt_flags & CAP_MEM_FROZEN_FLAG)) return -EINVAL;
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
		f = pgtbl_lkup_pte(((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, &flags);
		if (!f) return -ENOENT;
	//printk("%s-%s:%d %x\n", __FILE__, __func__, __LINE__, *f);
		old_v = *f;

		/* Cannot copy frame, or kernel entry. */
		if ((old_v & PGTBL_COSFRAME) || !(old_v & PGTBL_USER)) return -EPERM;
	//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
		/* TODO: validate the type is appropriate given the value of *flags */
		ret = pgtbl_mapping_add(((struct cap_pgtbl *)ctto)->pgtbl,
					capin_to, old_v & PGTBL_FRAME_MASK, flags);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static inline int
cap_move(struct captbl *t, capid_t cap_to, capid_t capin_to,
	capid_t cap_from, capid_t capin_from)
{
	struct cap_header *ctto, *ctfrom;
	int ret;
	cap_t cap_type;

	ctfrom = captbl_lkup(t, cap_from);
	if (unlikely(!ctfrom)) return -ENOENT;

	cap_type = ctfrom->type;

	if (cap_type == CAP_CAPTBL) {
		/* no cap copy needed yet. */
		return -EPERM;
	} else if (cap_type == CAP_PGTBL) {
		unsigned long *f, old_v, *moveto, old_v_to;
		u32_t flags;

		ctto = captbl_lkup(t, cap_to);
		if (unlikely(!ctto)) return -ENOENT;
		if (unlikely(ctto->type != cap_type)) return -EINVAL;
		if (unlikely(((struct cap_pgtbl *)ctto)->refcnt_flags & CAP_MEM_FROZEN_FLAG)) return -EINVAL;
		f = pgtbl_lkup_pte(((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, &flags);
		if (!f) return -ENOENT;
		old_v = *f;

		moveto = pgtbl_lkup_pte(((struct cap_pgtbl *)ctto)->pgtbl, capin_to, &flags);
		if (!moveto) return -ENOENT;
		old_v_to = *moveto;

		cos_mem_fence();
		if ((old_v & PGTBL_COSFRAME) == 0) return -EPERM;
		if (old_v_to & (PGTBL_COSFRAME | PGTBL_PRESENT)) return -EPERM;
		ret = pgtbl_quie_check(old_v_to);
		if (ret) return ret;

		/* valid to move. doing CAS next. */
		ret = cos_cas(f, old_v, 0);
		if (ret != CAS_SUCCESS) return -ECASFAIL;

		ret = cos_cas(moveto, old_v_to, old_v);
		if (ret != CAS_SUCCESS) {
			/* FIXME: reverse if the second cas fails. We
			 * should lock down the moveto slot first. */
			return -ECASFAIL;
		}
		ret = 0;
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int
cap_switch_thd(struct pt_regs *regs, struct thread *curr,
	       struct thread  *next, struct tcap *next_tcap,
	       struct comp_info *ci, struct cos_cpu_local_info *cos_info)
{
	struct comp_info *next_ci   = &(next->invstk[next->invstk_top].comp_info);
	struct tcap      *curr_tcap = tcap_current_update(cos_info);
	int               preempt   = 0;

	if (unlikely(curr == next)) {
		assert(!(curr->state & (THD_STATE_RCVING)));
		__userregs_set(regs, 0, __userregs_getsp(regs), __userregs_getip(regs));
		return 0;
	}

	assert(next_ci && curr && next);
	/* FIXME: trigger fault for the next thread, for now, return error */
	if (unlikely(!ltbl_isalive(&next_ci->liveness))) {
		__userregs_set(regs, -EFAULT, __userregs_getsp(regs), __userregs_getip(regs));
		return 0;
	}

	if (!(curr->state & THD_STATE_PREEMPTED)) {
		copy_gp_regs(regs, &curr->regs);
		__userregs_set(&curr->regs, 0, __userregs_getsp(&curr->regs), __userregs_getip(&curr->regs));
	} else {
		copy_all_regs(regs, &curr->regs);
	}

	thd_current_update(next, next_tcap, curr, cos_info);
	if (likely(ci->pgtbl != next_ci->pgtbl)) pgtbl_update(next_ci->pgtbl);

	/* Not sure of the trade-off here: Branch cost vs. segment register update */
	if (next->tls != curr->tls) chal_tls_update(next->tls);

	/* TODO: check FPU */
	/* fpu_save(thd); */
	if (next->state & THD_STATE_PREEMPTED) {
		assert(!(next->state & THD_STATE_RCVING));
		next->state &= ~THD_STATE_PREEMPTED;
		preempt = 1;
	} else if (next->state & THD_STATE_RCVING) {
		unsigned long a = 0, b = 0;

		assert(!(next->state & THD_STATE_PREEMPTED));
		next->state &= ~THD_STATE_RCVING;
		thd_state_evt_deliver(next, &a, &b);
		__userregs_setretvals(&next->regs, thd_rcvcap_pending_dec(next), a, b);
	}

	copy_all_regs(&next->regs, regs);

	return preempt;
}

static int
cap_thd_op(struct cap_thd *thd_cap, struct thread *thd, struct pt_regs *regs,
	   struct comp_info *ci, struct cos_cpu_local_info *cos_info)
{
	struct thread *next       = thd_cap->t;
	capid_t arcv              = (__userregs_get1(regs) << 16) >> 16;
	capid_t tc                = __userregs_get1(regs) >> 16;
	tcap_prio_t prio          = (tcap_prio_t)((tcap_prio_t)(__userregs_get3(regs) >> 16) << 32) | (tcap_prio_t)__userregs_get2(regs);
	tcap_res_t res            = (tcap_res_t)__userregs_get4(regs);
	sched_tok_t usr_counter   = (((sched_tok_t)__userregs_get3(regs) << 16) >> 16) | ((sched_tok_t)__userregs_getop(regs) << 16); /* op holds MSB of counter */
	struct tcap *tcap;

	if (thd_cap->cpuid != get_cpuid() || thd_cap->cpuid != next->cpuid) return -EINVAL;
	next->sw_counter = res;

	tcap = tcap_current(cos_info);
	if (arcv) {
		struct cap_arcv *arcv_cap;
		struct thread *rcvt;

		arcv_cap = (struct cap_arcv *)captbl_lkup(ci->captbl, arcv);
		if (!CAP_TYPECHK_CORE(arcv_cap, CAP_ARCV)) return -EINVAL;
		rcvt = arcv_cap->thd;
		/* race-condition check for user-level thread switches */
		assert(usr_counter < 0xFFFFFFFFU);
		if (thd_rcvcap_get_counter(rcvt) > usr_counter)	return -EAGAIN;
		thd_rcvcap_set_counter(rcvt, usr_counter);
		if (thd_rcvcap_pending(rcvt) > 0) {
			next = rcvt;
			/* tcap inheritance here...use the current tcap to process events */
			tc  = 0;
			res = 0;
		}
	}

	if (tc) {
		struct cap_tcap *tcap_cap;

		tcap_cap = (struct cap_tcap *)captbl_lkup(ci->captbl, tc);
		if (!CAP_TYPECHK_CORE(tcap_cap, CAP_TCAP)) return -EINVAL;
		tcap = tcap_cap->tcap;
	}

	return cap_switch_thd(regs, thd, next, tcap, ci, cos_info);
}

/**
 * Process the send event, and notify the appropriate end-points.
 * Return the thread that should be executed next.
 */
static struct thread *
asnd_process(struct thread *rcv_thd, struct thread *thd, struct tcap *rcv_tcap,
	     struct tcap *tcap, struct tcap **tcap_next, int yield)
{
	struct thread *next;
	struct thread *arcv_notif;

	thd_rcvcap_pending_inc(rcv_thd);

	arcv_notif = arcv_thd_notif(rcv_thd);
	if (arcv_notif) thd_rcvcap_evt_enqueue(arcv_notif, rcv_thd);

	next = rcv_thd;
	/* The thread switch decision: */
	/* if (tcap_higher_prio(rcv_tcap, tcap)) next = rcv_thd; */
	/* else                                  next = thd; */
	*tcap_next = tcap;

	return next;
}

static inline struct cap_arcv *
__cap_asnd_to_arcv(struct cap_asnd *asnd)
{
	struct cap_arcv *arcv;

	if (unlikely(!ltbl_isalive(&(asnd->comp_info.liveness)))) return NULL;
	arcv = (struct cap_arcv *)captbl_lkup(asnd->comp_info.captbl, asnd->arcv_capid);
	if (unlikely(!arcv || arcv->h.type != CAP_ARCV))          return NULL;
	/* FIXME: check arcv epoch + liveness */

	return arcv;
}

static int
cap_asnd_op(struct cap_asnd *asnd, struct thread *thd, struct pt_regs *regs,
	    struct comp_info *ci, struct cos_cpu_local_info *cos_info)
{
	int curr_cpu = get_cpuid();
	struct cap_arcv *arcv;
	struct thread *rcv_thd, *next;
	struct tcap *rcv_tcap, *tcap, *tcap_next;

	assert(asnd->arcv_capid);
	/* IPI notification to another core */
	if (asnd->arcv_cpuid != curr_cpu) return cos_cap_send_ipi(asnd->arcv_cpuid, asnd);
	arcv = __cap_asnd_to_arcv(asnd);
	if (unlikely(!arcv)) return -EINVAL;

	rcv_thd  = arcv->thd;
	tcap     = tcap_current(cos_info);
	rcv_tcap = rcv_thd->rcvcap.rcvcap_tcap;
	assert(rcv_tcap && tcap);

	next = asnd_process(rcv_thd, thd, rcv_tcap, tcap, &tcap_next, 0);

	return cap_switch_thd(regs, thd, next, tcap_next, ci, cos_info);
}

int
cap_hw_asnd(struct cap_asnd *asnd, struct pt_regs *regs)
{
	int restore_curr_thd = 1;
	int curr_cpu = get_cpuid();
	struct cap_arcv *arcv;
	struct cos_cpu_local_info *cos_info;
	struct thread *rcv_thd, *next, *thd;
	struct tcap *rcv_tcap, *tcap, *tcap_next;
	struct comp_info *ci;
	unsigned long ip, sp;

	if (!CAP_TYPECHK(asnd, CAP_ASND)) return restore_curr_thd;
	assert(asnd->arcv_capid);

	/* IPI notification to another core */
	if (asnd->arcv_cpuid != curr_cpu) {
		cos_cap_send_ipi(asnd->arcv_cpuid, asnd);
		return restore_curr_thd;
	}

	arcv       = __cap_asnd_to_arcv(asnd);
	if (unlikely(!arcv)) return restore_curr_thd;

	cos_info   = cos_cpu_local_info();
	assert(cos_info);
	thd        = thd_current(cos_info);
	tcap       = tcap_current(cos_info);
	assert(thd);
	ci         = thd_invstk_current(thd, &ip, &sp, cos_info);
	assert(ci  && ci->captbl);
	assert(!thd->state & THD_STATE_PREEMPTED);
	rcv_thd    = arcv->thd;
	rcv_tcap   = rcv_thd->rcvcap.rcvcap_tcap;
	assert(rcv_tcap && tcap);

	next       = asnd_process(rcv_thd, thd, rcv_tcap, tcap, &tcap_next, 0);
	if (next == thd) return restore_curr_thd;

	thd->state |= THD_STATE_PREEMPTED;
	return cap_switch_thd(regs, thd, next, tcap_next, ci, cos_info);
}

static int
cap_arcv_op(struct cap_arcv *arcv, struct thread *thd, struct pt_regs *regs,
	    struct comp_info *ci, struct cos_cpu_local_info *cos_info)
{
	struct thread *next;
	struct tcap   *tc = tcap_current(cos_info), *tc_next = tc;

	if (unlikely(arcv->thd != thd || arcv->cpuid != get_cpuid())) return -EINVAL;

	/* deliver pending notifications? */
	if (thd_rcvcap_pending(thd)) {
		unsigned long a = 0, b = 0;

		__userregs_set(regs, 0, __userregs_getsp(regs), __userregs_getip(regs));
		thd_state_evt_deliver(thd, &a, &b);
		__userregs_setretvals(regs, thd_rcvcap_pending_dec(thd), a, b);
		return 0;
	}

	/* FIXME:  for now, lets just ignore this path...need to plumb tcaps into it */
	thd->interrupted_thread = NULL;
	if (thd->interrupted_thread) {
		next = thd->interrupted_thread;
		assert(next->state & THD_STATE_PREEMPTED);
		thd->interrupted_thread = NULL;
		assert(0); 		/* need to take care of the tcap as well */
	} else {
		next = arcv_thd_notif(thd);
		/* don't change the current tcap: scheduler thread inherits it */
		/* root capability? */
		if (!next) return -EAGAIN;
		thd_rcvcap_evt_enqueue(next, thd);
	}

	if (likely(thd != next)) {
		assert(!(thd->state & THD_STATE_PREEMPTED));
		thd->state |= THD_STATE_RCVING;
	}

	return cap_switch_thd(regs, thd, next, tc_next, ci, cos_info);
}

static int
cap_introspect(struct captbl *ct, capid_t capid, u32_t op, unsigned long *retval)
{
	struct cap_header *ch = captbl_lkup(ct, capid);

	if (unlikely(!ch)) return -EINVAL;

	switch(ch->type) {
	case CAP_THD: return thd_introspect(((struct cap_thd*)ch)->t, op, retval);
	}
	return -EINVAL;
}

#define ENABLE_KERNEL_PRINT

static int
composite_syscall_slowpath(struct pt_regs *regs, int *thd_switch);

COS_SYSCALL __attribute__((section("__ipc_entry")))
int
composite_syscall_handler(struct pt_regs *regs)
{
	struct cap_header *ch;
	struct comp_info *ci;
	struct thread *thd;
	capid_t cap;
	unsigned long ip, sp;
	syscall_op_t op;
	/*
	 * We lookup this struct (which is on stack) only once, and
	 * pass it into other functions to avoid redundant lookups.
	 */
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	int ret = -ENOENT;
	int thd_switch = 0;

	cap = __userregs_getcap(regs);
	thd = thd_current(cos_info);

	/* printk("thd %d calling cap %d (ip %x, sp %x), operation %d: %x, %x, %x, %x\n", thd->tid, cap, */
	/*        __userregs_getip(regs), __userregs_getsp(regs), __userregs_getop(regs), */
	/*        __userregs_get1(regs), __userregs_get2(regs), __userregs_get3(regs), __userregs_get4(regs)); */


	/* fast path: invocation return (avoiding captbl accesses) */
	if (cap == COS_DEFAULT_RET_CAP) {
		/* No need to lookup captbl */
		sret_ret(thd, regs, cos_info);
		return 0;
	}

	/* FIXME: use a cap for print */
	if (unlikely(cap == PRINT_CAP_TEMP)) {
		printfn(regs);
		return 0;
	}

	ci = thd_invstk_current(thd, &ip, &sp, cos_info);
	assert(ci && ci->captbl);

	/*
	 * We don't check the liveness of the current component
	 * because it's guaranteed by component quiescence period,
	 * which is at timer tick granularity.
	 */
	ch = captbl_lkup(ci->captbl, cap);
	if (unlikely(!ch)) {
		printk("cos: cap %d not found!\n", (int)cap);
		cos_throw(done, 0);
	}
	/* fastpath: invocation */
	if (likely(ch->type == CAP_SINV)) {
		sinv_call(thd, (struct cap_sinv *)ch, regs, cos_info);
		return 0;
	}

	/*
	 * Some less common, but still optimized cases:
	 * thread dispatch, asnd and arcv operations.
	 */
	switch (ch->type) {
	case CAP_THD:
		ret = cap_thd_op((struct cap_thd *)ch, thd, regs, ci, cos_info);
		if (ret < 0) cos_throw(done, ret);
		return ret;
	case CAP_ASND:
		ret = cap_asnd_op((struct cap_asnd *)ch, thd, regs, ci, cos_info);
		if (ret < 0) cos_throw(done, ret);
		return ret;
	case CAP_ARCV:
		ret = cap_arcv_op((struct cap_arcv *)ch, thd, regs, ci, cos_info);
		if (ret < 0) cos_throw(done, ret);
		return ret;
	}

	/* slowpath restbl (captbl and pgtbl) operations */
	ret = composite_syscall_slowpath(regs, &thd_switch);
done:
	/*
	 * Note: we need to return ret to user-level (e.g. as a return
	 * value), which is not the return value of this function.
	 * Thus the level of indirection here.
	 */
	if (!thd_switch) __userregs_set(regs, ret, __userregs_getsp(regs), __userregs_getip(regs));

	return 0;
}

/*
 * slowpath: other capability operations, most of which
 * involve updating the resource tables.
 */
static int __attribute__((noinline))
composite_syscall_slowpath(struct pt_regs *regs, int *thd_switch)
{
	struct cap_header *ch;
	struct comp_info *ci;
	struct captbl *ct;
	struct thread *thd;
	capid_t cap,  capin;
	syscall_op_t op;
	int ret = -ENOENT;
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	unsigned long ip, sp;

	/*
	 * These variables are:
	 *
	 * ct:     the current captbl
	 * cap:    main capability looking up into the captbl
	 * capin:  capability to lookup *inside* that reftbl
	 * ch:     the header of cap in captbl
	 * op_cap: ch casted as captbl
	 * op:     operation to perform on the capability
	 */

	thd = thd_current(cos_info);
	cap = __userregs_getcap(regs);
	capin = __userregs_get1(regs);

	ci = thd_invstk_current(thd, &ip, &sp, cos_info);
	assert(ci && ci->captbl);
	ct = ci->captbl;

	ch = captbl_lkup(ct, cap);
	assert(ch);
	op = __userregs_getop(regs);

	switch(ch->type) {
	case CAP_CAPTBL:
	{
		struct cap_captbl *op_cap = (struct cap_captbl *)ch;

		/*
		 * FIXME: make sure that the lvl of the pgtbl makes
		 * sense for the op.
		 */
		switch(op) {
		case CAPTBL_OP_CAPTBLACTIVATE:
		{
			capid_t newcaptbl_cap = __userregs_get1(regs);
			capid_t pgtbl_cap     = __userregs_get2(regs);
			vaddr_t kmem_cap      = __userregs_get3(regs);
			int     captbl_lvl    = __userregs_get4(regs);

			struct captbl *newct;
			unsigned long *pte = NULL;
			vaddr_t kmem_addr  = 0;

			ret = cap_kmem_activate(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr, &pte);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr && pte);

			if (captbl_lvl == 0) {
				newct = captbl_create((void *)kmem_addr);
				assert(newct);
			} else {
				captbl_init((void *)kmem_addr, 1);
				captbl_init((void *)(kmem_addr+PAGE_SIZE/2), 1);
			}

			ret = captbl_activate(ct, cap, newcaptbl_cap, (struct captbl *)kmem_addr, captbl_lvl);
			if (ret) kmem_unalloc(pte);

			break;
		}
		case CAPTBL_OP_CAPTBLDEACTIVATE:
		{
			livenessid_t lid      = __userregs_get2(regs);

			ret = captbl_deactivate(ct, op_cap, capin, lid, 0, 0, 0);

			break;
		}
		case CAPTBL_OP_CAPTBLDEACTIVATE_ROOT:
		{
			livenessid_t lid      = __userregs_get2(regs);
			capid_t pgtbl_cap     = __userregs_get3(regs);
			capid_t cosframe_addr = __userregs_get4(regs);

			ret = captbl_deactivate(ct, op_cap, capin, lid, pgtbl_cap, cosframe_addr, 1);

			break;
		}
		case CAPTBL_OP_PGTBLACTIVATE:
		{
			capid_t pt_entry   = __userregs_get1(regs);
			capid_t pgtbl_cap  = __userregs_get2(regs);
			vaddr_t kmem_cap   = __userregs_get3(regs);
			capid_t pgtbl_lvl  = __userregs_get4(regs);

			pgtbl_t new_pt, curr_pt;
			vaddr_t kmem_addr  = 0;
			unsigned long *pte = NULL;

			ret = cap_kmem_activate(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr, &pte);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr && pte);

			if (pgtbl_lvl == 0) {
				/* PGD */
				struct cap_pgtbl *cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, pgtbl_cap);
				if (!CAP_TYPECHK(cap_pt, CAP_PGTBL)) {
					ret = -EINVAL;
					break;
				}

				curr_pt = cap_pt->pgtbl;
				assert(curr_pt);

				new_pt = pgtbl_create((void *)kmem_addr, curr_pt);
				ret = pgtbl_activate(ct, cap, pt_entry, new_pt, 0);
			} else if (pgtbl_lvl == 1) {
				/* PTE */
				pgtbl_init_pte((void *)kmem_addr);
				ret = pgtbl_activate(ct, cap, pt_entry, (pgtbl_t)kmem_addr, 1);
			} else {
				/* Not supported yet. */
				printk("cos: warning - PGTBL level greater than 2 not supported yet. \n");
				ret = -1;
			}

			if (ret) kmem_unalloc(pte);

			break;
		}
		case CAPTBL_OP_PGTBLDEACTIVATE:
		{
			livenessid_t lid      = __userregs_get2(regs);

			ret = pgtbl_deactivate(ct, op_cap, capin, lid, 0, 0, 0);

			break;
		}
		case CAPTBL_OP_PGTBLDEACTIVATE_ROOT:
		{
			livenessid_t lid      = __userregs_get2(regs);
			capid_t pgtbl_cap     = __userregs_get3(regs);
			capid_t cosframe_addr = __userregs_get4(regs);

			ret = pgtbl_deactivate(ct, op_cap, capin, lid, pgtbl_cap, cosframe_addr, 1);

			break;
		}
		case CAPTBL_OP_THDACTIVATE:
		{
			capid_t thd_cap    = __userregs_get1(regs) & 0xFFFF;
			int init_data      = __userregs_get1(regs) >> 16;
			capid_t pgtbl_cap  = __userregs_get2(regs);
			capid_t pgtbl_addr = __userregs_get3(regs);
			capid_t compcap    = __userregs_get4(regs);

			struct thread *thd;
			unsigned long *pte = NULL;

			ret = cap_kmem_activate(ct, pgtbl_cap, pgtbl_addr, (unsigned long *)&thd, &pte);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(thd && pte);

			/* ret is returned by the overall function */
			ret = thd_activate(ct, cap, thd_cap, thd, compcap, init_data);
			if (ret) kmem_unalloc(pte);

			break;
		}
		case CAPTBL_OP_TCAP_ACTIVATE:
		{
			capid_t tcap_cap   = __userregs_get1(regs);
			capid_t pgtbl_cap  = __userregs_get2(regs);
			capid_t pgtbl_addr = __userregs_get3(regs);
			struct tcap   *tcap;
			unsigned long *pte = NULL;

			ret = cap_kmem_activate(ct, pgtbl_cap, pgtbl_addr, (unsigned long *)&tcap, &pte);
			if (unlikely(ret)) cos_throw(err, ret);

			ret = tcap_activate(ct, cap, tcap_cap, tcap);
			if (ret) kmem_unalloc(pte);

			break;
		}
		case CAPTBL_OP_THDDEACTIVATE:
		{
			livenessid_t lid = __userregs_get2(regs);

			ret = thd_deactivate(ct, op_cap, capin, lid, 0, 0, 0);
			break;
		}
		case CAPTBL_OP_THDTLSSET:
		{
			capid_t thd_cap = __userregs_get1(regs);
			vaddr_t tlsaddr = __userregs_get2(regs);

			assert(op_cap->captbl);
			if (thd_tls_set(op_cap->captbl, thd_cap, tlsaddr, thd)) cos_throw(err, -EINVAL);
			break;
		}
		case CAPTBL_OP_THDDEACTIVATE_ROOT:
		{
			livenessid_t lid      = __userregs_get2(regs);
			capid_t pgtbl_cap     = __userregs_get3(regs);
			capid_t cosframe_addr = __userregs_get4(regs);

			ret = thd_deactivate(ct, op_cap, capin, lid, pgtbl_cap, cosframe_addr, 1);
			break;
		}
		case CAPTBL_OP_CAPKMEM_FREEZE:
		{
			capid_t freeze_cap     = capin;

			ret = cap_kmem_freeze(op_cap->captbl, freeze_cap);
			break;
		}
		case CAPTBL_OP_COMPACTIVATE:
		{
			capid_t captbl_cap = __userregs_get2(regs) >> 16;
			capid_t pgtbl_cap  = __userregs_get2(regs) & 0xFFFF;
			livenessid_t lid   = __userregs_get3(regs);
			vaddr_t entry_addr = __userregs_get4(regs);

			ret = comp_activate(ct, cap, capin, captbl_cap, pgtbl_cap, lid, entry_addr, NULL);
			break;
		}
		case CAPTBL_OP_COMPDEACTIVATE:
		{
			livenessid_t lid  = __userregs_get2(regs);

			ret = comp_deactivate(op_cap, capin, lid);
			break;
		}
		case CAPTBL_OP_SINVACTIVATE:
		{
			capid_t dest_comp_cap = __userregs_get2(regs);
			vaddr_t entry_addr    = __userregs_get3(regs);

			ret = sinv_activate(ct, cap, capin, dest_comp_cap, entry_addr);
			break;
		}
		case CAPTBL_OP_SINVDEACTIVATE:
		{
			livenessid_t lid  = __userregs_get2(regs);

			ret = sinv_deactivate(op_cap, capin, lid);
			break;
		}
		case CAPTBL_OP_SRETACTIVATE:
		{
			ret = -EINVAL;
			break;
		}
		case CAPTBL_OP_SRETDEACTIVATE:
		{
			livenessid_t lid  = __userregs_get2(regs);

			ret = sret_deactivate(op_cap, capin, lid);
			break;
		}
		case CAPTBL_OP_ASNDACTIVATE:
		{
			capid_t rcv_captbl = __userregs_get2(regs);
			capid_t rcv_cap    = __userregs_get3(regs);

			ret = asnd_activate(ct, cap, capin, rcv_captbl, rcv_cap, 0, 0);
			break;
		}
		case CAPTBL_OP_ASNDDEACTIVATE:
		{
			livenessid_t lid  = __userregs_get2(regs);

			ret = asnd_deactivate(op_cap, capin, lid);
			break;
		}
		case CAPTBL_OP_ARCVACTIVATE:
		{
			capid_t thd_cap  = (__userregs_get2(regs) << 16) >> 16;
			capid_t tcap_cap = __userregs_get2(regs) >> 16;
			capid_t comp_cap = __userregs_get3(regs);
			capid_t arcv_cap = __userregs_get4(regs);

			ret = arcv_activate(ct, cap, capin, comp_cap, thd_cap, tcap_cap, arcv_cap, 0);
			break;
		}
		case CAPTBL_OP_ARCVDEACTIVATE:
		{
			livenessid_t lid  = __userregs_get2(regs);

			ret = arcv_deactivate(op_cap, capin, lid);
			break;
		}
		case CAPTBL_OP_CPY:
		{
			capid_t from_captbl = cap;
			capid_t from_cap    = __userregs_get1(regs);
			capid_t dest_captbl = __userregs_get2(regs);
			capid_t dest_cap    = __userregs_get3(regs);

			ret = cap_cpy(ct, dest_captbl, dest_cap,
				      from_captbl, from_cap);
			break;
		}
		case CAPTBL_OP_CONS:
		{
			capid_t cons_addr = __userregs_get2(regs);

			ret = cap_cons(ct, cap, capin, cons_addr);
			break;
		}
		case CAPTBL_OP_DECONS:
		{
			capid_t decons_addr = __userregs_get2(regs);
			capid_t lvl         = __userregs_get3(regs);

			/* FIXME: adding liveness id here. */

			ret = cap_decons(ct, cap, capin, decons_addr, lvl);

			break;
		}
		case CAPTBL_OP_INTROSPECT:
		{
			struct captbl *ctin  = op_cap->captbl;
			unsigned long retval = 0;
			u32_t op             = __userregs_get2(regs);
			assert(ctin);

			ret = cap_introspect(ctin, capin, op, &retval);
			if (!ret) ret = retval;
			
			break;
		}
		case CAPTBL_OP_HW_ACTIVATE:
		{
			u32_t bitmap = __userregs_get2(regs);

			ret = hw_activate(ct, cap, capin, bitmap);
			break;
		}
		case CAPTBL_OP_HW_DEACTIVATE:
		{
			livenessid_t lid  = __userregs_get2(regs);

			ret = hw_deactivate(op_cap, capin, lid);
			break;
		}
		default: goto err;
		}
		break;
	}
	case CAP_PGTBL:
	{
		/* pgtbl_t pt = ((struct cap_pgtbl *)ch)->pgtbl; */
		capid_t pt = cap;

		switch (op) {
		case CAPTBL_OP_CPY:
		{
			capid_t source_pt   = pt;
			vaddr_t source_addr = __userregs_get1(regs);
			capid_t dest_pt     = __userregs_get2(regs);
			vaddr_t dest_addr   = __userregs_get3(regs);

			//printk("%s-%s:%d src:%x dst:%x\n", __FILE__, __func__, __LINE__, source_addr, dest_addr);
			ret = cap_cpy(ct, dest_pt, dest_addr, source_pt, source_addr);

			break;
		}
		case CAPTBL_OP_MEMMOVE:
		{
			/* Moves a mem frame to another pgtbl. Used to
			 * grant frames to memory management
			 * components.  */
			capid_t source_pt   = pt;
			vaddr_t source_addr = __userregs_get1(regs);
			capid_t dest_pt     = __userregs_get2(regs);
			vaddr_t dest_addr   = __userregs_get3(regs);

			ret = cap_move(ct, dest_pt, dest_addr, source_pt, source_addr);

			break;
		}
		case CAPTBL_OP_CONS:
		{
			vaddr_t pte_cap   = __userregs_get1(regs);
			vaddr_t cons_addr = __userregs_get2(regs);

			ret = cap_cons(ct, pt, pte_cap, cons_addr);

			break;
		}
		case CAPTBL_OP_DECONS:
		{
			capid_t decons_addr = __userregs_get2(regs);
			capid_t lvl         = __userregs_get3(regs);

			ret = cap_decons(ct, cap, capin, decons_addr, lvl);

			break;
		}
		case CAPTBL_OP_MEMACTIVATE:
		{
			/* This takes cosframe as input and constructs
			 * mapping in pgtbl. */
			capid_t frame_cap = __userregs_get1(regs);
			capid_t dest_pt   = __userregs_get2(regs);
			vaddr_t vaddr     = __userregs_get3(regs);

			ret = cap_memactivate(ct, (struct cap_pgtbl *)ch, frame_cap, dest_pt, vaddr);

			break;
		}
		case CAPTBL_OP_MEMDEACTIVATE:
		{
			vaddr_t addr      = __userregs_get1(regs);
			livenessid_t lid  = __userregs_get2(regs);

			if (((struct cap_pgtbl *)ch)->lvl) cos_throw(err, -EINVAL);

			ret = pgtbl_mapping_del(((struct cap_pgtbl *)ch)->pgtbl, addr, lid);

			break;
		}
		case CAPTBL_OP_MEM_RETYPE2USER:
		{
			//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
			vaddr_t frame_addr = __userregs_get1(regs);
			paddr_t frame;

			//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
			ret = pgtbl_get_cosframe(((struct cap_pgtbl *)ch)->pgtbl, frame_addr, &frame);
			if (ret) cos_throw(err, ret);

			//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
			ret = retypetbl_retype2user((void *)frame);

			break;
		}
		case CAPTBL_OP_MEM_RETYPE2KERN:
		{
			//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
			vaddr_t frame_addr = __userregs_get1(regs);
			paddr_t frame;

			//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
			ret = pgtbl_get_cosframe(((struct cap_pgtbl *)ch)->pgtbl, frame_addr, &frame);
			if (ret) cos_throw(err, ret);

			//printk("%s-%s:%d\n", __FILE__, __func__, __LINE__);
			ret = retypetbl_retype2kern((void *)frame);

			break;
		}
		case CAPTBL_OP_MEM_RETYPE2FRAME:
		{
			vaddr_t frame_addr = __userregs_get1(regs);
			paddr_t frame;

			ret = pgtbl_get_cosframe(((struct cap_pgtbl *)ch)->pgtbl, frame_addr, &frame);
			if (ret) cos_throw(err, ret);

			ret = retypetbl_retype2frame((void *)frame);

			break;
		}
		case CAPTBL_OP_INTROSPECT:
		{
			vaddr_t addr = __userregs_get1(regs);
			unsigned long *pte;
			u32_t flags;

			pte = pgtbl_lkup_pte(((struct cap_pgtbl *)ch)->pgtbl, addr, &flags);

			if (pte) ret = *pte;
			else 	 ret = 0;

			break;
		}
		/* case CAPTBL_OP_MAPPING_MOD: */
		default: goto err;
		}
		break;
	}
	case CAP_SRET:
	{
		/*
		 * We usually don't have sret cap as we have 0 as the
		 * default return cap.
		 */
		sret_ret(thd, regs, cos_info);
		return 0;
	}
	case CAP_TCAP:
	{
		/* TODO: Validate that all tcaps are on the same core */
		switch (op){
		case CAPTBL_OP_TCAP_TRANSFER:
		{
			capid_t tcpdst 		 = __userregs_get1(regs);
			long long res 		 = __userregs_get2(regs);
			u32_t prio_higher 	 = __userregs_get3(regs);
			u32_t prio_lower 	 = __userregs_get4(regs);
			tcap_prio_t prio 	 = (tcap_prio_t)prio_higher << 32 | (tcap_prio_t)prio_lower;
			struct cap_tcap *tcapsrc = (struct cap_tcap *)ch;
			struct cap_tcap *tcapdst;

			tcapdst = (struct cap_tcap *)captbl_lkup(ci->captbl, tcpdst);
			if (!CAP_TYPECHK_CORE(tcapdst, CAP_TCAP)) cos_throw(err, -EINVAL);

			ret = tcap_transfer(tcapdst->tcap, tcapsrc->tcap, res, prio);
			if (unlikely(ret)) cos_throw(err, -EINVAL);

			break;
		}
		case CAPTBL_OP_TCAP_DELEGATE:
		{
			capid_t asnd_cap  = __userregs_get1(regs);
			long long res 	  = __userregs_get2(regs);
			u32_t prio_higher = __userregs_get3(regs);
			u32_t prio_lower  = __userregs_get4(regs);
			tcap_prio_t prio;
			struct cap_tcap *tcapsrc = (struct cap_tcap *)ch;
			struct cap_arcv *arcv;
			struct cap_asnd *asnd;
			struct thread   *rthd;
			struct tcap     *tcapdst, *tcap_next;
			int yield;
			struct thread *n;

			/* highest-order bit is dispatch flag */
			yield       = prio_higher >> ((sizeof(prio_higher)*8)-1);
			prio_higher = (prio_higher << 1) >> 1;
			prio        = (tcap_prio_t)prio_higher << 32 | (tcap_prio_t)prio_lower;
			asnd        = (struct cap_asnd *)captbl_lkup(ci->captbl, asnd_cap);
			if (!CAP_TYPECHK(asnd, CAP_ASND)) cos_throw(err, -EINVAL);

			arcv = __cap_asnd_to_arcv(asnd);
			rthd = arcv->thd;
			assert(rthd && rthd->rcvcap.rcvcap_tcap);
			tcapdst = rthd->rcvcap.rcvcap_tcap;

			ret = tcap_delegate(tcapsrc->tcap, tcapdst, res, prio);
			if (unlikely(ret)) cos_throw(err, -EINVAL);

			n = asnd_process(rthd, thd, tcapdst, tcap_current(cos_info), &tcap_next, yield);
			if (n != thd) {
				ret = cap_switch_thd(regs, thd, n, tcap_next, ci, cos_info);
				*thd_switch = 1;
			}

			break;
		}
		case CAPTBL_OP_TCAP_MERGE:
		{
			capid_t tcaprem		 = __userregs_get1(regs);
			struct cap_tcap *tcapdst = (struct cap_tcap *)ch;
			struct cap_tcap *tcaprm;

			tcaprm = (struct cap_tcap *)captbl_lkup(ci->captbl, tcaprem);
			if (!CAP_TYPECHK_CORE(tcaprm, CAP_TCAP)) cos_throw(err, -EINVAL);

			ret = tcap_merge(tcapdst->tcap, tcaprm->tcap);
			if (unlikely(ret)) cos_throw(err, -ENOENT);

			break;
		}
		default: goto err;
		}

	}
	case CAP_HW:
	{
		switch(op) {
		case CAPTBL_OP_HW_ATTACH:
		{
			struct cap_arcv *rcvc;
			hwid_t hwid    = __userregs_get1(regs);
			capid_t rcvcap = __userregs_get2(regs);

			rcvc = (struct cap_arcv *)captbl_lkup(ci->captbl, rcvcap);
			if (!CAP_TYPECHK(rcvc, CAP_ARCV)) cos_throw(err, -EINVAL);

			ret = hw_attach_rcvcap((struct cap_hw *)ch, hwid, rcvc, rcvcap);
			break;
		}
		case CAPTBL_OP_HW_DETACH:
		{
			hwid_t hwid = __userregs_get1(regs);

			ret = hw_detach_rcvcap((struct cap_hw *)ch, hwid);
			break;
		}
		case CAPTBL_OP_HW_MAP:
		{
			capid_t ptcap = __userregs_get1(regs);
			vaddr_t va    = __userregs_get2(regs);
			paddr_t pa    = __userregs_get3(regs);
			struct cap_pgtbl *ptc;
			unsigned long    *pte;
			u32_t             flags;

			/*
			 * FIXME: This is broken.  It should only be
			 * used on physical address that are not
			 * accessible through the normal mechanisms
			 * (i.e. on physical apertures presented by
			 * PCI).
			 */
			ptc = (struct cap_pgtbl *)captbl_lkup(ci->captbl, ptcap);
			if (!CAP_TYPECHK(ptc, CAP_PGTBL)) cos_throw(err, -EINVAL);

			pte = pgtbl_lkup_pte(ptc->pgtbl, va, &flags);
			if (!pte)                    cos_throw(err, -EINVAL);
			if (*pte & PGTBL_FRAME_MASK) cos_throw(err, -ENOENT);
			*pte = (PGTBL_FRAME_MASK & pa) | PGTBL_USER_DEF;

			ret = 0;
			break;
		}
		case CAPTBL_OP_HW_CYC_USEC:
		{
			ret = chal_cyc_usec();
			break;
		}
		default: goto err;
		}
		break;
	}
	default: break;
	}
err:
	return ret;
}
