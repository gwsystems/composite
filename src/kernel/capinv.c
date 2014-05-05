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

#define COS_DEFAULT_RET_CAP 0

#ifdef LINUX_TEST
int
syscall_handler(struct pt_regs *regs)
#else

#ifndef COS_SYSCALL
#define COS_SYSCALL __attribute__((regparm(0)))
#endif

static inline void
fs_reg_setup(unsigned long seg) {
	asm volatile ("movl %%ebx, %%fs\n\t"
		      : : "b" (seg));
}

#define ENABLE_KERNEL_PRINT
#define MAX_LEN 512

static inline int printfn(struct pt_regs *regs) 
{
	char *str; 
	int len;
	char kern_buf[MAX_LEN];

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


__attribute__((section("__ipc_entry"))) COS_SYSCALL int
composite_sysenter_handler(struct pt_regs *regs)
#endif
{
	struct cap_header *ch;
	struct comp_info *ci;
	struct thread *thd;
	capid_t cap;
	unsigned long ip, sp;
	syscall_op_t op;
	int ret;

#ifdef ENABLE_KERNEL_PRINT
	fs_reg_setup(__KERNEL_PERCPU);
#endif
	cap = __userregs_getcap(regs);
	thd = thd_current();

	if (likely(cap == COS_DEFAULT_RET_CAP)) {
		/* fast path: invocation return */
		sret_ret(thd, regs);
		return 0;
	}

#ifndef LINUX_TEST
	if (regs->ax == PRINT_CAP_TEMP) {
		printfn(regs);
		return 0;
	}
#endif

	ci  = thd_invstk_current(thd, &ip, &sp);
	assert(ci && ci->captbl);
	/* TODO: check liveness map */

	ch  = captbl_lkup(ci->captbl, cap);
	if (unlikely(!ch)) {
		ret = -ENOENT;
		goto done;
	}

	/* fastpath: invocation */
	if (likely(ch->type == CAP_SINV)) {
		sinv_call(thd, (struct cap_sinv *)ch, regs);
		return 0;
	} 

	if (cap > 2) {
		printk("calling cap %x!!!, print %x\n", cap, PRINT_CAP_TEMP);
		return 0;
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
	__userregs_setret(regs, ret);

	return 0;
}
