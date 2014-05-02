/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/captbl.h"
#include "include/inv.h"
#include "include/thread.h"
#include "include/call_convention.h"

void
syscall_handler(struct pt_regs *regs)
{
	struct cap_header *ch;
	struct comp_info *ci;
	struct thread *thd;
	capid_t cap;
	unsigned long ip, sp;
	syscall_op_t op;

	thd = thd_current();
	ci  = thd_invstk_current(thd, &ip, &sp);
	assert(ci && ci->captbl);
	/* TODO: check liveness map */
	cap = __userregs_getcap(regs); 	/* FIXME */
	ch  = captbl_lkup(ci->captbl, cap);
	if (unlikely(!ch)) {
		regs->ax = -ENOENT;
		return;
	}

	/* fastpath: invocation and return */
	if (likely(ch->type == CAP_SINV)) {
		sinv_call(thd, (struct cap_sinv *)ch, regs);
		return;
	} else if (likely(ch->type == CAP_SRET)) {
		sret_ret(thd, regs);
		return;
	}

	op = __userregs_getop(regs);
	/* slowpath: other capability operations */
	switch(ch->type) {
	case CAP_ASND: 		/* FIXME: add asynchronous sending */
	case CAP_ARCV: 		/* FIXME: add asynchronous receive */
	case CAP_THD: 		/* FIXME: add thread dispatch */
		goto err;
	case CAP_CAPTBL:
	{
		capid_t capin = __userregs_get1(regs);
		int ret;
		/* 
		 * FIXME: make sure that the lvl of the pgtbl makes
		 * sense for the op.
		 */
		switch(op) {
		case CAPTBL_OP_THDACTIVATE:
		{
			capid_t pgtbl_cap  = __userregs_get2(regs);
			capid_t pgtbl_addr = __userregs_get3(regs);
			capid_t compcap    = __userregs_get4(regs);
			struct thread *thd;

			ret = cap_mem_retype2kern(ci->captbl, pgtbl_cap, pgtbl_addr, (unsigned long *)&thd);
			if (unlikely(ret)) cos_throw(err, ret);
			ret = thd_activate(ci->captbl, cap, capin, thd, compcap);
			/* ret is returned by the overall function */
		}
		case CAPTBL_OP_THDDEACTIVATE:
			/* 
			 * FIXME: move the thread capability to a
			 * location in a pagetable as COSFRAME
			 */
			ret = thd_deactivate(ci->captbl, cap, capin);
			break;
		case CAPTBL_OP_COMPACTIVATE:
		{
			capid_t comp_cap   = capin;
			capid_t captbl_cap = __userregs_get2(regs) >> 16;
			capid_t pgtbl_cap  = __userregs_get2(regs) & 0xFFFF;
			livenessid_t lid   = __userregs_get3(regs);
			vaddr_t entry_addr = __userregs_get4(regs);

			ret = comp_activate(ci->captbl, cap, comp_cap, captbl_cap, pgtbl_cap, lid, entry_addr, NULL);
		}
		case CAPTBL_OP_COMPDEACTIVATE:
			ret = comp_deactivate(ci->captbl, cap, capin);
			break;

		case CAPTBL_OP_SINVACTIVATE:
		case CAPTBL_OP_SINVDEACTIVATE:
			ret = sinv_deactivate(ci->captbl, cap, capin);
			break;
		case CAPTBL_OP_SRETACTIVATE:
		case CAPTBL_OP_SRETDEACTIVATE:
			ret = sret_deactivate(ci->captbl, cap, capin);
			break;

		case CAPTBL_OP_ASNDACTIVATE:
		case CAPTBL_OP_ASNDDEACTIVATE:
			ret = asnd_deactivate(ci->captbl, cap, capin);
			break;
		case CAPTBL_OP_ARCVACTIVATE:
		case CAPTBL_OP_ARCVDEACTIVATE:
			ret = arcv_deactivate(ci->captbl, cap, capin);
			break;

		case CAPTBL_OP_CPY:
		case CAPTBL_OP_CONS:
		case CAPTBL_OP_DECONS:
		default: goto err;
		}
		break;
	}
	case CAP_PGTBL:
		switch (op) {
		case CAPTBL_OP_CPY:
		case CAPTBL_OP_CONS:
		case CAPTBL_OP_DECONS:
		case CAPTBL_OP_MAPPING_CONS:
		case CAPTBL_OP_MAPPING_DECONS:
		case CAPTBL_OP_MAPPING_MOD:
		case CAPTBL_OP_MAPPING_RETYPE:
		default: goto err;
		}
	default:
	err:
		ret = -ENOENT;
	}
	__userregs_setret(regs, ret);

	return;
}
