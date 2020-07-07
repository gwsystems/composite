#ifndef CALL_CONVENTION_H
#define CALL_CONVENTION_H

#include "../../../kernel/include/asm_ipc_defs.h"

/*
 * Functions to maintain calling conventions on invocation and return
 * (i.e. to make sure the registers are appropriately set up).
 */
static inline void
__userregs_setret(struct pt_regs *regs, unsigned long ret)
{
	regs->r5 = ret;
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
	return (regs->r0 >> COS_CAPABILITY_OFFSET) - 1;
}
static inline u32_t
__userregs_getop(struct pt_regs *regs)
{
	return regs->r0 & ((1 << COS_CAPABILITY_OFFSET) - 1);
}
static inline unsigned long
__userregs_getinvret(struct pt_regs *regs)
{
	return regs->r1;
} /* cx holds the return value on invocation return path. */
static inline void
__userregs_set(struct pt_regs *regs, unsigned long ret, unsigned long sp, unsigned long ip)
{
	regs->r5 = ret;
	regs->r13_sp = sp;
	regs->r15_pc = ip;
}
static inline void
__userregs_setretvals(struct pt_regs *regs, unsigned long ret, unsigned long ret1, unsigned long ret2, unsigned long ret3)
{
	regs->r5 = ret;
	regs->r0 = ret1;
	regs->r1 = ret2;
	regs->r2 = ret3;
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
	return regs->r1;
}
static inline int
__userregs_get2(struct pt_regs *regs)
{
	return regs->r2;
}
static inline int
__userregs_get3(struct pt_regs *regs)
{
	return regs->r3;
}
static inline int
__userregs_get4(struct pt_regs *regs)
{
	return regs->r4;
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
regs_upcall_setup(struct pt_regs *regs, u32_t entry_addr, int option, int id, int arg1, int arg2, int arg3)
{
	regs->r0 = option;

	regs->r2 = arg1;
	regs->r3 = arg2;
	regs->r4 = arg3;

	regs->r15_pc = entry_addr;
	regs->r1 = id;
printk("!!! id is %x\n",id);
	return;
}

static inline void
regs_print(struct pt_regs* regs)
{
	printk("\tregs - 0x%x\n",regs);
	printk("\tcpsr - 0x%x\n",regs->cpsr);
	printk("\tr0 - 0x%x\n",regs->r0);
	printk("\tr1 - 0x%x\n",regs->r1);
	printk("\tr2 - 0x%x\n",regs->r2);
	printk("\tr3- 0x%x\n",regs->r3);
	printk("\tr4 - 0x%x\n",regs->r4);
	printk("\tr5 - 0x%x\n",regs->r5);
	printk("\tr6 - 0x%x\n",regs->r6);
	printk("\tr7 - 0x%x\n",regs->r7);
	printk("\tr8 - 0x%x\n",regs->r8);
	printk("\tr9 - 0x%x\n",regs->r9);
	printk("\tr10 - 0x%x\n",regs->r10);
	printk("\tr11 - 0x%x\n",regs->r11);
	printk("\tr12 - 0x%x\n",regs->r12);
	printk("\tr13_sp - 0x%x\n",regs->r13_sp);
	printk("\tr14_lr - 0x%x\n",regs->r14_lr);
	printk("\tr15_pc - 0x%x\n",regs->r15_pc);
}
#endif /* CALL_CONVENTION_H */
