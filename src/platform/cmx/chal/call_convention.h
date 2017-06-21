#ifndef CALL_CONVENTION_H
#define CALL_CONVENTION_H

#include "../../../kernel/include/asm_ipc_defs.h"
/* Register usage:
 * r0 - cap_op
 * r1 - arg1
 * r2 - arg2
 * r3 - arg3
 * r4 - arg4
 * r5 - ret
 * r6 - fault
 * r7 - ret1
 * r8 - ret2
 */

struct pt_regs
{
	int flags_xpsr;
	int r15_pc;
	int r14_lr;
	int r13_sp;
	int r0;
	int r1;
	int r2;
	int r3;
	int r4;
	int r5;
	int r6;
	int r7;
	int r8;
	int r9;
	int r10;
	int r11;
	int r12;
};

/*
 * Functions to maintain calling conventions on invocation and return
 * (i.e. to make sure the registers are appropriately set up).
 */
static inline void
__userregs_setret(struct pt_regs *regs, unsigned long ret)
{ regs->r5 = ret; }
static inline unsigned long
__userregs_getsp(struct pt_regs *regs)
{ return regs->r13_sp; }
static inline unsigned long
__userregs_getip(struct pt_regs *regs)
{ return regs->r15_pc;}
static inline capid_t
__userregs_getcap(struct pt_regs *regs)
{ return (regs->r0 >> COS_CAPABILITY_OFFSET) - 1;}
static inline u32_t
__userregs_getop(struct pt_regs *regs)
{ return regs->r0 & ((1<<COS_CAPABILITY_OFFSET) - 1);}
static inline unsigned long
__userregs_getinvret(struct pt_regs *regs)
{ return regs->r1;} /* r1 hold the return value on invocation return path. */
static inline void
__userregs_set(struct pt_regs *regs, unsigned long ret, unsigned long r13_sp, unsigned long r15_pc)
{
	regs->r5 = ret;
	regs->r13_sp /* = regs->r1 */= r13_sp;
	regs->r15_pc /* = regs->r2 */= r15_pc;
}
static inline void
__userregs_setretvals(struct pt_regs *regs, unsigned long ret, unsigned long ret1, unsigned long ret2)
{
	regs->r5 = ret;
	regs->r7 = ret1;
	regs->r8 = ret2;
}
static inline void
__userregs_sinvupdate(struct pt_regs *regs)
{
	/* IPC calling side has 4 args (in order): r0, r1, r2, r3 */
	/* IPC server side receives 4 args: r4, r6, r7, r8 (r5 is used for retval passing) */
	/* So we need to pass all the arguments. */
	regs->r4 = regs->r0;
	regs->r6 = regs->r1;
	regs->r7 = regs->r2;
	regs->r8 = regs->r3;
}
static inline int
__userregs_get1(struct pt_regs *regs)
{ return regs->r1; }
static inline int
__userregs_get2(struct pt_regs *regs)
{ return regs->r2; }
static inline int
__userregs_get3(struct pt_regs *regs)
{ return regs->r3; }
static inline int
__userregs_get4(struct pt_regs *regs)
{ return regs->r4; }

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
	COPY_REG(flags_xpsr);
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
	COPY_REG(flags_xpsr);
#undef COPY_REG
}

#endif	/* CALL_CONVENTION_H */
