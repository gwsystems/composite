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
	struct captbl *ct;
	struct thread *thd;
	capid_t cap;
	unsigned long ip, sp;
	syscall_op_t op;
	int ret = 0;

#ifndef LINUX_TEST
#ifdef ENABLE_KERNEL_PRINT
	fs_reg_setup(__KERNEL_PERCPU);
#endif
	if (regs->ax == PRINT_CAP_TEMP) {
		printfn(regs);
		return 0;
	}
#endif
	cap = __userregs_getcap(regs);
	thd = thd_current();

	if (likely(cap == COS_DEFAULT_RET_CAP)) {
		/* fast path: invocation return */
		sret_ret(thd, regs);
		return 0;
	}

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

	op = __userregs_getop(regs);
	ct  = ci->captbl; 

	/* printk("calling cap %d, op %d: %x, %x, %x, %x\n", */
	/*        cap, op, __userregs_get1(regs), __userregs_get2(regs), __userregs_get3(regs), __userregs_get4(regs)); */

	/* slowpath: other capability operations */
	switch(ch->type) {
	case CAP_ASND: 		/* FIXME: add asynchronous sending */
	{
		break;
	}
	case CAP_ARCV: 		/* FIXME: add asynchronous receive */
	{
		break;
	}
	case CAP_THD: 		/* FIXME: add thread dispatch */
	{
		break;
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
		case CAPTBL_OP_SRETDEACTIVATE:
			ret = sret_deactivate(ct, cap, capin);
			break;

		case CAPTBL_OP_ASNDACTIVATE:
		case CAPTBL_OP_ASNDDEACTIVATE:
			ret = asnd_deactivate(ct, cap, capin);
			break;
		case CAPTBL_OP_ARCVACTIVATE:
		case CAPTBL_OP_ARCVDEACTIVATE:
			ret = arcv_deactivate(ct, cap, capin);
			break;

		case CAPTBL_OP_CPY:
		case CAPTBL_OP_CONS:
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
			vaddr_t source_addr = __userregs_get1(regs);
			capid_t dest_pt     = __userregs_get2(regs);
			vaddr_t dest_addr   = __userregs_get3(regs);
			vaddr_t mem_addr;

			/* Fixme.... */
			ret = cap_mem_retype2kern(ct, pt, source_addr, (unsigned long *)&mem_addr);
			if (unlikely(ret)) cos_throw(err, ret);
			assert(mem_addr);

			ret = cap_memactivate(ct, dest_pt, dest_addr, 
					      chal_va2pa(mem_addr), PGTBL_USER_DEF);

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
