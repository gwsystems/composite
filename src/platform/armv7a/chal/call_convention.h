#ifndef CALL_CONVENTION_H
#define CALL_CONVENTION_H

#include "../../../kernel/include/asm_ipc_defs.h"

/*
 * Calling convention registers here..
 *
 * input:
 *	r1 = operation & capability for system call.
 *      r2, r3, r4, r5 = arguments.
 *
 * return:
 *	r0 = return value
 *      r2, r3, r4 = multiple return value for sinv ret.
 *
 *      r0 = contains thdid + cpuid for thd upcall & sinv.
 *      r1 = option for thd upcall, token for sinv
 */


/*
 * Functions to maintain calling conventions on invocation and return
 * (i.e. to make sure the registers are appropriately set up).
 */
static inline void
__userregs_setret(struct pt_regs *regs, unsigned long ret)
{
	regs->r0 = ret;
}

static inline unsigned long
__userregs_getsp(struct pt_regs *regs)
{
	return regs->r13_sp;
}

static inline unsigned long
__userregs_getip(struct pt_regs *regs)
{
	return regs->r15_pc;
}

static inline capid_t
__userregs_getcap(struct pt_regs *regs)
{
	return (regs->r1 >> COS_CAPABILITY_OFFSET) - 1;
}

static inline u32_t
__userregs_getop(struct pt_regs *regs)
{
	return regs->r1 & ((1 << COS_CAPABILITY_OFFSET) - 1);
}

static inline unsigned long
__userregs_getinvret(struct pt_regs *regs)
{
	return regs->r0;
} 

static inline void
__userregs_setinv(struct pt_regs *regs, unsigned long id, unsigned long tok, unsigned long ip)
{
	regs->r0 = id;
	regs->r1 = tok;
	regs->r15_pc = ip;
}

static inline void
__userregs_set(struct pt_regs *regs, unsigned long ret, unsigned long sp, unsigned long ip)
{
	regs->r0 = ret;
	regs->r13_sp = sp;
	regs->r15_pc = ip;
}

static inline void
__userregs_setretvals(struct pt_regs *regs, unsigned long ret, unsigned long ret1, unsigned long ret2, unsigned long ret3)
{
	regs->r0 = ret;
	regs->r2 = ret1;
	regs->r3 = ret2;
	regs->r4 = ret3;
}

static inline void
__userregs_sinvupdate(struct pt_regs *regs)
{
	/* IPC calling side has 4 args (in order): r1 r2 r3 r4 */
	/* IPC server side receives 4 args: r1, r2, r3, r4 */
	/* No movement needed */
	/* regs->r1 = regs->r1; */
	/* regs->r2 = regs->r2; */
	/* regs->r3 = regs->r3; */
	/* regs->r4 = regs->r5; */
}

static inline int
__userregs_get1(struct pt_regs *regs)
{
	return regs->r2;
}

static inline int
__userregs_get2(struct pt_regs *regs)
{
	return regs->r3;
}

static inline int
__userregs_get3(struct pt_regs *regs)
{
	return regs->r4;
}

static inline int
__userregs_get4(struct pt_regs *regs)
{
	return regs->r5;
}

static inline void
copy_gp_regs(struct pt_regs *from, struct pt_regs *to)
{
#define COPY_REG(reg) to->reg = from->reg
	COPY_REG(r0);
	COPY_REG(r1);
	COPY_REG(r2);
	COPY_REG(r3);
	COPY_REG(r4);
	COPY_REG(r5);
	COPY_REG(r6);
	COPY_REG(r7);
	COPY_REG(r8);
	COPY_REG(r9);
	COPY_REG(r10);
	COPY_REG(r11);
	COPY_REG(r12);
	COPY_REG(r13_sp);
	COPY_REG(r14_lr);
	COPY_REG(r15_pc);
	COPY_REG(cpsr);
#undef COPY_REG
}

static inline void
copy_all_regs(struct pt_regs *from, struct pt_regs *to)
{
#define COPY_REG(reg) to->reg = from->reg
	COPY_REG(r0);
	COPY_REG(r1);
	COPY_REG(r2);
	COPY_REG(r3);
	COPY_REG(r4);
	COPY_REG(r5);
	COPY_REG(r6);
	COPY_REG(r7);
	COPY_REG(r8);
	COPY_REG(r9);
	COPY_REG(r10);
	COPY_REG(r11);
	COPY_REG(r12);
	COPY_REG(r13_sp);
	COPY_REG(r14_lr);
	COPY_REG(r15_pc);
	COPY_REG(cpsr);
#undef COPY_REG
}

static inline void
regs_print(struct pt_regs* regs)
{
	printk("Register state - 0x%x: \n",regs);
	printk("\tcpsr: 0x%x, r0: 0x%x, r1: 0x%x, r2: 0x%x, r3: 0x%x, r4: 0x%x\n",
			regs->cpsr, regs->r0, regs->r1, regs->r2, regs->r3, regs->r4);
	printk("\tr5: 0x%x, r6: 0x%x, r7: 0x%x, r8: 0x%x, r9: 0x%x, r10: 0x%x\n",
			regs->r5, regs->r6, regs->r7, regs->r8, regs->r9, regs->r10);
	printk("\tr11: 0x%x, r12: 0x%x, r13 (sp): 0x%x, r14 (lr): 0x%x, r15 (pc): 0x%x\n",
			regs->r11, regs->r12, regs->r13_sp, regs->r14_lr, regs->r15_pc);
}

static inline void
regs_upcall_setup(struct pt_regs *regs, u32_t entry_addr, int option, int id, int arg1, int arg2, int arg3)
{
	regs->r0 = id;

	regs->cpsr = CPSR_USER_LEVEL; 
	regs->r1 = option;
	regs->r2 = arg1;
	regs->r3 = arg2;
	regs->r4 = arg3;

	regs->r15_pc = entry_addr;

	return;
}

#endif /* CALL_CONVENTION_H */
