/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/captbl.h"
#include "include/inv.h"
#include "include/thd.h"
#include "include/call_convention.h"
#include "include/ipi_cap.h"

#define COS_DEFAULT_RET_CAP 0

static inline void
fs_reg_setup(unsigned long seg) {
#ifdef LINUX_TEST
	return;
#endif
	asm volatile ("movl %%ebx, %%fs\n\t"
		      : : "b" (seg));
}

#ifdef LINUX_TEST
int
syscall_handler(struct pt_regs *regs)
#else

#ifndef COS_SYSCALL
#define COS_SYSCALL __attribute__((regparm(0)))
#endif

#define ENABLE_KERNEL_PRINT
#define MAX_LEN 512

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
	kern_buf[len] = '\0';
	printk("%s", kern_buf);
done:
	__userregs_set(regs, 0, __userregs_getsp(regs), __userregs_getip(regs));

	return 0;
}

static int
cap_switch_thd(struct pt_regs *regs, struct thread *curr, struct thread *next) 
{
	int preempt = 0;
	struct comp_info *next_ci = &(next->invstk[next->invstk_top].comp_info);

	copy_gp_regs(regs, &curr->regs);
	__userregs_set(&curr->regs, COS_SCHED_RET_SUCCESS, __userregs_getsp(regs), __userregs_getip(regs));

	thd_current_update(next);
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

__attribute__((section("__ipc_entry"))) COS_SYSCALL int
composite_sysenter_handler(struct pt_regs *regs)
#endif
{
	struct cap_header *ch;
	struct comp_info *ci;
	struct captbl *ct;
	struct thread *thd;
	capid_t cap;
	unsigned long ip, sp;
	syscall_op_t op;
	int ret = 0;

#ifdef ENABLE_KERNEL_PRINT
	fs_reg_setup(__KERNEL_PERCPU);
#endif
#ifndef LINUX_TEST
	if (regs->ax == PRINT_CAP_TEMP) {
		printfn(regs);
		return 0;
	}
#endif
	cap = __userregs_getcap(regs);
	thd = thd_current();
	/* printk("calling cap %d: %x, %x, %x, %x\n", */
	/*        cap, __userregs_get1(regs), __userregs_get2(regs), __userregs_get3(regs), __userregs_get4(regs)); */

	// remove likely?
	/* fast path: invocation return */
	if (likely(cap == COS_DEFAULT_RET_CAP)) {
		/* No need to lookup captbl */
		sret_ret(thd, regs);
		return 0;
	}

	ci  = thd_invstk_current(thd, &ip, &sp);
	assert(ci && ci->captbl);
	/* TODO: check liveness map */

	ch  = captbl_lkup(ci->captbl, cap);
	if (unlikely(!ch)) {
		printk("cos: cap %d not found!\n", cap);
		ret = -ENOENT;
		goto done;
	}

	/* fastpath: invocation */
	if (likely(ch->type == CAP_SINV)) {
		sinv_call(thd, (struct cap_sinv *)ch, regs);
		return 0;
	}

	op = __userregs_getop(regs);
	ct  = ci->captbl; 

	/* printk("calling cap %d, op %d: %x, %x, %x, %x\n", */
	/*        cap, op, __userregs_get1(regs), __userregs_get2(regs), __userregs_get3(regs), __userregs_get4(regs)); */
#ifndef ENABLE_KERNEL_PRINT
	fs_reg_setup(__KERNEL_PERCPU);
#endif
	/* slowpath: other capability operations */
	switch(ch->type) {
	case CAP_ASND:
	{
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

			//FIXME: check liveness!
			arcv = (struct cap_arcv *)captbl_lkup(asnd->comp_info.captbl, asnd->arcv_capid);
			if (unlikely(arcv->h.type != CAP_ARCV)) {
				printk("cos: IPI handling received invalid arcv cap %d\n", asnd->arcv_capid);
				cos_throw(err, EINVAL);
			}
			
			return cap_switch_thd(regs, thd, arcv->thd);
		}
				
		break;
	}
	case CAP_ARCV:
	{
		struct cap_arcv *arcv = (struct cap_arcv *)ch;

		/*FIXME: add epoch checking!*/

		if (arcv->thd != thd) {
			cos_throw(err, EINVAL);
		}

		/* Sanity checks */
		assert(arcv->cpuid == get_cpuid());
		assert(arcv->comp_info.pgtbl = ci->pgtbl);
		assert(arcv->comp_info.captbl = ci->captbl);

		if (arcv->pending) {
			arcv->pending--;
			ret = 0;

			break;
		}
		
		if (thd->interrupted_thread == NULL) {
			/* FIXME: handle this case by upcalling into scheduler. */
			ret = -1;
			printk("fixmefixmefixme!!!\n");
		} else {
			thd->arcv_cap = cap;
			thd->flags &= !THD_STATE_ACTIVE_UPCALL;
			thd->flags |= THD_STATE_READY_UPCALL;
			
			return cap_switch_thd(regs, thd, thd->interrupted_thread);
		}
		
		break;
	}
	case CAP_THD:
	{
		struct cap_thd *thd_cap = (struct cap_thd *)ch;
		struct thread *next = thd_cap->t;

		if (thd_cap->cpuid != get_cpuid()) cos_throw(err, EINVAL);
		assert(thd_cap->cpuid == next->cpuid);

		// TODO: check liveness tbl
	
		// QW: hack!!! for ppos test only. remove!
		next->interrupted_thread = thd;

		return cap_switch_thd(regs, thd, next);
	}
	case CAP_CAPTBL:
	{
		capid_t capin =  __userregs_get1(regs);
		/* 
		 * FIXME: make sure that the lvl of the pgtbl makes
		 * sense for the op.
		 */
		switch(op) {
		case CAPTBL_OP_CAPTBLACTIVATE:
		{
			capid_t pgtbl_cap      = __userregs_get1(regs);
			vaddr_t kmem_cap       = __userregs_get2(regs);
			capid_t newcaptbl_cap  = __userregs_get3(regs);
			vaddr_t kmem_addr = 0;
			struct captbl *newct;
			
			ret = cap_mem_retype2kern(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr);

			newct = captbl_create(kmem_addr);
			assert(newct);
			ret = captbl_activate(ct, cap, newcaptbl_cap, newct, 0);

			break;
		}
		case CAPTBL_OP_PGDACTIVATE:
		{
			capid_t pgtbl_cap  = __userregs_get1(regs);
			vaddr_t kmem_cap   = __userregs_get2(regs);
			capid_t newpgd_cap = __userregs_get3(regs);
			vaddr_t kmem_addr  = 0;
			pgtbl_t new_pt, curr_pt;
			struct cap_pgtbl *pt;

			ret = cap_mem_retype2kern(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr);

			curr_pt = ((struct cap_pgtbl *)captbl_lkup(ct, pgtbl_cap))->pgtbl;
			assert(curr_pt);

			new_pt = pgtbl_create(kmem_addr, curr_pt);
			ret = pgtbl_activate(ct, cap, newpgd_cap, new_pt, 0);

			break;
		}
		case CAPTBL_OP_PTEACTIVATE:
		{
			capid_t pgtbl_cap  = __userregs_get1(regs);
			vaddr_t kmem_cap   = __userregs_get2(regs);
			capid_t newpte_cap = __userregs_get3(regs);
			vaddr_t kmem_addr  = 0;

			ret = cap_mem_retype2kern(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(kmem_addr);

			pgtbl_init_pte(kmem_addr);
			ret = pgtbl_activate(ct, cap, newpte_cap, (pgtbl_t)kmem_addr, 1);

			break;
		}
		case CAPTBL_OP_THDACTIVATE:
		{
			capid_t pgtbl_cap  = __userregs_get2(regs);
			capid_t pgtbl_addr = __userregs_get3(regs);
			capid_t compcap    = __userregs_get4(regs);
			struct thread *thd;

			ret = cap_mem_retype2kern(ct, pgtbl_cap, pgtbl_addr, (unsigned long *)&thd);
			if (unlikely(ret)) cos_throw(err, ret);

			ret = thd_activate(ct, cap, capin, thd, compcap);
			/* ret is returned by the overall function */
			break;
		}
		case CAPTBL_OP_THDDEACTIVATE:
			/* 
			 * FIXME: move the thread capability to a
			 * location in a pagetable as COSFRAME
			 */
			ret = thd_deactivate(ct, cap, capin);
			break;
		case CAPTBL_OP_COMPACTIVATE:
		{
			capid_t captbl_cap = __userregs_get2(regs) >> 16;
			capid_t pgtbl_cap  = __userregs_get2(regs) & 0xFFFF;
			livenessid_t lid   = __userregs_get3(regs);
			vaddr_t entry_addr = __userregs_get4(regs);

			ret = comp_activate(ct, cap, capin, captbl_cap, pgtbl_cap, lid, entry_addr, NULL);
			/* printk("ret %d, comp act @ cap %x, ct cap %x, pt cap %x, lid %x, entry %x\n",  */
			/*        ret, comp_cap, captbl_cap, pgtbl_cap, lid, entry_addr); */
			break;
		}
		case CAPTBL_OP_COMPDEACTIVATE:
			ret = comp_deactivate(ct, cap, capin);
			break;
		case CAPTBL_OP_SINVACTIVATE:
		{
			capid_t dest_comp_cap = __userregs_get2(regs);
			vaddr_t entry_addr    = __userregs_get3(regs);
			
			ret = sinv_activate(ct, cap, capin, dest_comp_cap, entry_addr);

			break;
		}
		case CAPTBL_OP_SINVDEACTIVATE:
			ret = sinv_deactivate(ct, cap, capin);
			break;
		case CAPTBL_OP_SRETACTIVATE:
			break;
		case CAPTBL_OP_SRETDEACTIVATE:
			ret = sret_deactivate(ct, cap, capin);
			break;

		case CAPTBL_OP_ASNDACTIVATE:
		{
			capid_t rcv_captbl = __userregs_get2(regs);
			capid_t rcv_cap    = __userregs_get3(regs);
			ret = asnd_activate(ct, cap, capin, rcv_captbl, rcv_cap, 0, 0);

			break;
		}
		case CAPTBL_OP_ASNDDEACTIVATE:
			ret = asnd_deactivate(ct, cap, capin);
			break;
		case CAPTBL_OP_ARCVACTIVATE:
		{
			capid_t thd_cap  = __userregs_get2(regs);
			capid_t comp_cap = __userregs_get3(regs);

			ret = arcv_activate(ct, cap, capin, comp_cap, thd_cap);
			
			break;
		}
		case CAPTBL_OP_ARCVDEACTIVATE:
			ret = arcv_deactivate(ct, cap, capin);
			break;

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
			capid_t target      = cap;
			capid_t target_id   = capin;
			capid_t pgtbl_cap   = __userregs_get2(regs);
			capid_t pgtbl_addr  = __userregs_get3(regs);
			void *captbl_mem;
			struct cap_captbl *target_ct;

			ret = cap_mem_retype2kern(ct, pgtbl_cap, pgtbl_addr, (unsigned long *)&captbl_mem);
			if (unlikely(ret)) cos_throw(err, ret);

			target_ct = (struct cap_captbl *)captbl_lkup(ct, target);
			if (target_ct->h.type != CAP_CAPTBL) cos_throw(err, EINVAL);

			captbl_init(captbl_mem, 1);
			ret = captbl_expand(target_ct->captbl, target_id, captbl_maxdepth(), captbl_mem);
			assert(!ret);

			captbl_init(&((char*)captbl_mem)[PAGE_SIZE/2], 1);
			ret = captbl_expand(target_ct->captbl, target_id + (PAGE_SIZE/2/CAPTBL_LEAFSZ), 
					    captbl_maxdepth(), &((char*)captbl_mem)[PAGE_SIZE/2]);
			assert(!ret);

			break;
		}
		case CAPTBL_OP_DECONS:
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
			vaddr_t pa;

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
		case CAPTBL_OP_MAPPING_CONS:
		{
			break;
		}
		case CAPTBL_OP_MAPPING_DECONS:
		case CAPTBL_OP_MAPPING_MOD:
		case CAPTBL_OP_MAPPING_RETYPE:
		default: goto err;
		}
		break;
	}
	case CAP_SRET: 
	{
		sret_ret(thd, regs);
		return 0;
	}
	default:
	err:
		ret = -ENOENT;
	}
done:
	__userregs_set(regs, ret, __userregs_getsp(regs), __userregs_getip(regs));

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

	// scan the first half
	for (; idx < NUM_CPU; idx++) {
		ring = &receiver_rings->IPI_source[idx];
		if (ring->sender != ring->receiver) {
			process_ring(ring);
		}
	}

	//scan the second half
	for (idx = 0; idx <= end; idx++) {
		ring = &receiver_rings->IPI_source[idx];
		if (ring->sender != ring->receiver) {
			process_ring(ring);
		}
	}

	return;
}
