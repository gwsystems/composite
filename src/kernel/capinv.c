/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/inv.h"
#include "include/thd.h"
#include "include/call_convention.h"
#include "include/ipi_cap.h"
#include "include/liveness_tbl.h"
#include "include/cpuid.h"

#define COS_DEFAULT_RET_CAP 0

#ifdef LINUX_TEST

#include <stdio.h>

static inline int printfn(struct pt_regs *regs) {
	__userregs_set(regs, 0, __userregs_getsp(regs), __userregs_getip(regs));
	return 0;
}

static inline void
fs_reg_setup(unsigned long seg) {
	(void)seg;
	return;
}

int
syscall_handler(struct pt_regs *regs)

#else

static inline void
fs_reg_setup(unsigned long seg) {
	asm volatile ("movl %%ebx, %%fs\n\t"
		      : : "b" (seg));
}

#define MAX_LEN 512
extern char timer_detector[PAGE_SIZE] PAGE_ALIGNED;
static inline int printfn(struct pt_regs *regs) 
{
	char *str; 
	int len;
	char kern_buf[MAX_LEN];

	fs_reg_setup(__KERNEL_PERCPU);

	str     = (char *)__userregs_get1(regs);
	len     = __userregs_get2(regs);

	if (len < 1) goto done;
	if (len >= MAX_LEN) len = MAX_LEN - 1;
	memcpy(kern_buf, str, len);

	if (len >= 6) { //well, hack to flush tlb and cache...
		if (kern_buf[0] == 'F' && kern_buf[1] == 'L' && kern_buf[2] == 'U' &&
		    kern_buf[3] == 'S' && kern_buf[4] == 'H' && kern_buf[5] == '!') {
			u32_t *ticks;
			chal_flush_cache();
			chal_flush_tlb_global();
			ticks = (u32_t *)&timer_detector[get_cpuid() * CACHE_LINE];
//			printk("inv ticks %u\n", *ticks);

			__userregs_set(regs, *ticks, 
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

void cos_cap_ipi_handling(void)
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

/* 
 * Copy a capability from a location in one captbl/pgtbl to a location
 * in the other.  Fundamental operation used to delegate capabilities.
 * TODO: should limit the types of capabilities this works on.
 */
static inline int
cap_cpy(struct captbl *t, capid_t cap_to, capid_t capin_to, 
	capid_t cap_from, capid_t capin_from)
{
	struct cap_header *ctto, *ctfrom;
	int sz, ret;
	cap_t cap_type;
	
	/* printk("copy from captbl %d, cap %d to captbl %d, cap %d\n", */
	/*        cap_from, capin_from, cap_to, capin_to); */
	ctfrom = captbl_lkup(t, cap_from);
	if (unlikely(!ctfrom)) return -ENOENT;

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
		unsigned long *f, old_v;
		u32_t flags;

		ctto = captbl_lkup(t, cap_to);
		if (unlikely(!ctto)) return -ENOENT;
		if (unlikely(ctto->type != cap_type)) return -EINVAL;
		if (unlikely(((struct cap_pgtbl *)ctto)->refcnt_flags & CAP_MEM_FROZEN_FLAG)) return -EINVAL;

		f = pgtbl_lkup_pte(((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, &flags);
		if (!f) return -ENOENT;
		old_v = *f;

		/* Cannot copy frame, or kernel entry. */
		if ((old_v & PGTBL_COSFRAME) || !(old_v & PGTBL_USER)) return -EPERM;
		
		/* TODO: validate the type is appropriate given the value of *flags */
		ret = pgtbl_mapping_add(((struct cap_pgtbl *)ctto)->pgtbl, 
					capin_to, old_v & PGTBL_FRAME_MASK, flags);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int
cap_switch_thd(struct pt_regs *regs, struct thread *curr, struct thread *next, struct cos_cpu_local_info *cos_info) 
{
	int preempt = 0;
	struct comp_info *next_ci = &(next->invstk[next->invstk_top].comp_info);

	if (unlikely(!ltbl_isalive(&next_ci->liveness))) {
		printk("cos: comp (liveness %d) doesn't exist!\n", next_ci->liveness.id);
		//FIXME: add fault handling here.
		__userregs_set(regs, -EFAULT, __userregs_getsp(regs), __userregs_getip(regs));
		return preempt;
	}
	
	copy_gp_regs(regs, &curr->regs);
	__userregs_set(&curr->regs, COS_SCHED_RET_SUCCESS, __userregs_getsp(regs), __userregs_getip(regs));

	thd_current_update(next, curr, cos_info);

	pgtbl_update(next_ci->pgtbl);

	/* fpu_save(thd); */
	if (next->flags & THD_STATE_PREEMPTED) {
		cos_meas_event(COS_MEAS_SWITCH_PREEMPT);
		/* remove_preempted_status(thd); */
		next->flags &= ~THD_STATE_PREEMPTED;
		preempt = 1;
	}
//	printk("Core %d: switching from %d to thd %d, preempted %d\n", get_cpuid(), curr->tid, next->tid, preempt);
		
	/* update_sched_evts(thd, thd_sched_flags, curr, curr_sched_flags); */
	/* event_record("switch_thread", thd_get_id(thd), thd_get_id(next)); */
	copy_gp_regs(&next->regs, regs);

	return preempt;
}

#define ENABLE_KERNEL_PRINT

__attribute__((section("__ipc_entry"))) COS_SYSCALL int
composite_sysenter_handler(struct pt_regs *regs)
#endif
{
	struct cap_header *ch;
	struct comp_info *ci;
	struct captbl *ct;
	struct thread *thd;
	capid_t cap, capin;
	unsigned long ip, sp;
	syscall_op_t op;
	/* We lookup this struct (which is on stack) only once, and
	 * pass it into other functions to avoid unnecessary
	 * lookup. */
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	int ret = 0;

#ifdef ENABLE_KERNEL_PRINT
	fs_reg_setup(__KERNEL_PERCPU);
#endif
	/* printk("calling cap %d: %x, %x, %x, %x\n", */
	/*        cap, __userregs_get1(regs), __userregs_get2(regs), __userregs_get3(regs), __userregs_get4(regs)); */

	cap = __userregs_getcap(regs);

	thd = thd_current(cos_info);

	/* fast path: invocation return */
	if (cap == COS_DEFAULT_RET_CAP) {
		/* No need to lookup captbl */
		sret_ret(thd, regs, cos_info);
		return 0;
	}

	/* FIXME: use a cap for print */
	if (unlikely(regs->ax == PRINT_CAP_TEMP)) {
		printfn(regs);
		return 0;
	}

	ci = thd_invstk_current(thd, &ip, &sp, cos_info);
	assert(ci && ci->captbl);

	/* We don't check liveness of current component because it's
	 * guaranteed by component quiescence period, which is at
	 * timer tick granularity.*/
	ch = captbl_lkup(ci->captbl, cap);
	if (unlikely(!ch)) {
		printk("cos: cap %d not found!\n", (int)cap);
		ret = -ENOENT;
		goto done;
	}
	/* fastpath: invocation */
	if (likely(ch->type == CAP_SINV)) {
		sinv_call(thd, (struct cap_sinv *)ch, regs, cos_info);
		return 0;
	}

	/* Some less common cases: thread dispatch, asnd and arcv
	 * operations. */
	if (ch->type == CAP_THD) {
		struct cap_thd *thd_cap = (struct cap_thd *)ch;
		struct thread *next = thd_cap->t;

		if (thd_cap->cpuid != get_cpuid()) cos_throw(err, -EINVAL);
		assert(thd_cap->cpuid == next->cpuid);

		// QW: hack!!! for ppos test only. remove!
		next->interrupted_thread = thd;

		return cap_switch_thd(regs, thd, next, cos_info);
	} else if (ch->type == CAP_ASND) {
		int curr_cpu = get_cpuid();
		struct cap_asnd *asnd = (struct cap_asnd *)ch;

		assert(asnd->arcv_capid);

		if (asnd->arcv_cpuid != curr_cpu) {
			/* Cross core: sending IPI */
			ret = cos_cap_send_ipi(asnd->arcv_cpuid, asnd);
			/* printk("sending ipi to cpu %d. ret %d\n", asnd->arcv_cpuid, ret); */
		} else {
			struct cap_arcv *arcv;

			printk("NOT tested yet.\n");

			if (unlikely(!ltbl_isalive(&(asnd->comp_info.liveness)))) {
				// fault handle? 
				cos_throw(err, -EFAULT);
			}

			arcv = (struct cap_arcv *)captbl_lkup(asnd->comp_info.captbl, asnd->arcv_capid);
			if (unlikely(arcv->h.type != CAP_ARCV)) {
				printk("cos: IPI handling received invalid arcv cap %d\n", asnd->arcv_capid);
				cos_throw(err, -EINVAL);
			}
			
			return cap_switch_thd(regs, thd, arcv->thd, cos_info);
		}
				
		goto done;
	} else if (ch->type == CAP_ARCV) {
		struct cap_arcv *arcv = (struct cap_arcv *)ch;

		/*FIXME: add epoch checking!*/

		if (arcv->thd != thd) {
			cos_throw(err, -EINVAL);
		}

		/* Sanity checks */
		assert(arcv->cpuid == get_cpuid());
		assert(arcv->comp_info.pgtbl = ci->pgtbl);
		assert(arcv->comp_info.captbl = ci->captbl);

		if (arcv->pending) {
			arcv->pending--;
			ret = 0;

			goto done;
		}
		
		if (thd->interrupted_thread == NULL) {
			/* FIXME: handle this case by upcalling into
			 * scheduler, or switch to a scheduling
			 * thread. */
			ret = -1;
			printk("ERROR: not implemented yet!\n");
		} else {
			thd->arcv_cap = cap;
			thd->flags &= !THD_STATE_ACTIVE_UPCALL;
			thd->flags |= THD_STATE_READY_UPCALL;
			
			return cap_switch_thd(regs, thd, thd->interrupted_thread, cos_info);
		}
		
		goto done;
	}

	fs_reg_setup(__KERNEL_PERCPU);

	/* slowpath: other capability operations, most of which
	 * involve writing. */
	op = __userregs_getop(regs);
	ct = ci->captbl; 
	capin = __userregs_get1(regs);

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
			capid_t newcaptbl_cap  = __userregs_get1(regs);
			capid_t pgtbl_cap      = __userregs_get2(regs);
			vaddr_t kmem_cap       = __userregs_get3(regs);
			int     captbl_lvl     = __userregs_get4(regs);
			vaddr_t kmem_addr = 0;
			struct captbl *newct;
			
			ret = cap_kmem_activate(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr);

			newct = captbl_create((void *)kmem_addr);
			assert(newct);
			ret = captbl_activate(ct, cap, newcaptbl_cap, newct, captbl_lvl);

			break;
		}
		case CAPTBL_OP_CAPTBLDEACTIVATE:
		{
			/* TODO: use two different cases for
			 * deactivation w/ and w/o kmem
			 * deactivation. */

			capid_t pgtbl_cap     = __userregs_get2(regs);
			capid_t cosframe_addr = __userregs_get3(regs);
			livenessid_t lid      = __userregs_get4(regs);

			ret = captbl_deactivate(ct, op_cap, capin, lid, pgtbl_cap, cosframe_addr);
			
			break;
		}
		case CAPTBL_OP_PGTBLACTIVATE:
		{
			capid_t pt_entry   = __userregs_get1(regs);
			capid_t pgtbl_cap  = __userregs_get2(regs);
			vaddr_t kmem_cap   = __userregs_get3(regs);
			capid_t pgtbl_lvl  = __userregs_get4(regs);

			vaddr_t kmem_addr  = 0;
			pgtbl_t new_pt, curr_pt;

			ret = cap_kmem_activate(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr);

			if (pgtbl_lvl == 0) {
				/* PGD */
				struct cap_pgtbl *cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, pgtbl_cap);
				if (cap_pt->h.type != CAP_PGTBL) {
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

			break;
		}
		case CAPTBL_OP_PGTBLDEACTIVATE:
		{
			/* TODO: use two different cases for
			 * deactivation w/ and w/o kmem
			 * deactivation. */
			capid_t pgtbl_cap     = __userregs_get2(regs);
			capid_t cosframe_addr = __userregs_get3(regs);
			livenessid_t lid      = __userregs_get4(regs);

			ret = pgtbl_deactivate(ct, op_cap, capin, lid, pgtbl_cap, cosframe_addr);
			
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

			ret = cap_kmem_activate(ct, pgtbl_cap, pgtbl_addr, (unsigned long *)&thd);
			if (unlikely(ret)) cos_throw(err, ret);

			ret = thd_activate(ct, cap, thd_cap, thd, compcap, init_data);
			/* ret is returned by the overall function */

			break;
		}
		case CAPTBL_OP_THDDEACTIVATE:
		{
			capid_t pgtbl_cap     = __userregs_get2(regs);
			capid_t cosframe_addr = __userregs_get3(regs);
			livenessid_t lid      = __userregs_get4(regs);

			ret = thd_deactivate(ct, op_cap, capin, lid, pgtbl_cap, cosframe_addr);
			break;
		}
		case CAPTBL_OP_CAPKMEM_FREEZE:
		{
			capid_t freeze_cap     = __userregs_get2(regs);

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
			printk("Error: No activation for SRET implementation yet!\n");
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
			capid_t thd_cap  = __userregs_get2(regs);
			capid_t comp_cap = __userregs_get3(regs);

			ret = arcv_activate(ct, cap, capin, comp_cap, thd_cap);
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

			ret = cap_decons(ct, cap, capin, decons_addr, lvl);

			break;
		}
		default: goto err;
		}
		break;
	}
	case CAP_PGTBL:
	{
		capid_t pt = cap;

		switch (op) {
		case CAPTBL_OP_CPY:
		{
			capid_t source_pt   = pt;
			vaddr_t source_addr = __userregs_get1(regs);
			capid_t dest_pt     = __userregs_get2(regs);
			vaddr_t dest_addr   = __userregs_get3(regs);

			ret = cap_cpy(ct, dest_pt, dest_addr, source_pt, source_addr);

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
			vaddr_t frame_addr = __userregs_get1(regs);
			paddr_t frame;

			ret = pgtbl_get_cosframe(((struct cap_pgtbl *)ch)->pgtbl, frame_addr, &frame);
			if (ret) cos_throw(err, ret);

			ret = retypetbl_retype2user((void *)frame);

			break;
		}
		case CAPTBL_OP_MEM_RETYPE2KERN:
		{
			vaddr_t frame_addr = __userregs_get1(regs);
			paddr_t frame;

			ret = pgtbl_get_cosframe(((struct cap_pgtbl *)ch)->pgtbl, frame_addr, &frame);
			if (ret) cos_throw(err, ret);

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
		/* case CAPTBL_OP_MAPPING_MOD: */
		/* { */
		/* } */
		default: goto err;
		}
		break;
	}
	case CAP_SRET:
	{
		/* We usually don't have sret cap as we have 0 as the
		 * default return cap.*/
		sret_ret(thd, regs, cos_info);
		return 0;
	}
	default:
	err:
		if (ret == 0) ret = -ENOENT;
	}
done:
	__userregs_set(regs, ret, __userregs_getsp(regs), __userregs_getip(regs));

	return 0;
}

