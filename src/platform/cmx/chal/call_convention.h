#ifndef CALL_CONVENTION_H
#define CALL_CONVENTION_H

#include "../../../kernel/include/asm_ipc_defs.h"

/*
 * Functions to maintain calling conventions on invocation and return
 * (i.e. to make sure the registers are appropriately set up).
 */
static inline void
__userregs_setret(struct pt_regs *regs, unsigned long ret)
{ regs->r4 = ret; }
static inline unsigned long
__userregs_getsp(struct pt_regs *regs)
{ return regs->r12; }
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
{ return regs->r2;} /* r2 holr4 the return value on invocation return path. */
static inline void
__userregs_set(struct pt_regs *regs, unsigned long ret, unsigned long r13_sp, unsigned long r15_pc)
{
	regs->r4 = ret;
	regs->r13_sp = regs->r2 = r13_sp;
	regs->r15_pc = regs->r3 = r15_pc;
}
static inline void
__userregs_setretvals(struct pt_regs *regs, unsigned long ret, unsigned long ret1, unsigned long ret2)
{
	regs->r4 = ret;
	regs->r10 = ret1;
	regs->r11 = ret2;
}
static inline void
__userregs_sinvupdate(struct pt_regs *regs)
{
	/* IPC calling side has 4 args (in order): r1, r10, r11, r3 */
	/* IPC server side receives 4 args: r1, r10, r11, r12 */
	/* So we need to pass the 4th argument. */
	/*regs->r1 = regs->r1;
	regs->r10 = regs->r10;
	regs->r11 = regs->r11;*/
	regs->r12 = regs->r3;
}
static inline int
__userregs_get1(struct pt_regs *regs)
{ return regs->r1; }
static inline int
__userregs_get2(struct pt_regs *regs)
{ return regs->r10; }
static inline int
__userregs_get3(struct pt_regs *regs)
{ return regs->r11; }
static inline int
__userregs_get4(struct pt_regs *regs)
{ return regs->r3; }

static inline void
copy_gp_regs(struct pt_regs *from, struct pt_regs *to)
{
#define COPY_REG(reg) to->reg = from->reg
	COPY_REG(r0);
	COPY_REG(r1);
	COPY_REG(r2);
	COPY_REG(r3);
	COPY_REG(r10);
	COPY_REG(r11);
	COPY_REG(r12);
#undef COPY_REG
}

static inline void
copy_all_regs(struct pt_regs *from, struct pt_regs *to)
{
#define COPY_REG(reg) to->reg = from->reg
	COPY_REG(r1);
	COPY_REG(r2);
	COPY_REG(r3);
	COPY_REG(r10);
	COPY_REG(r11);
	COPY_REG(r12);
	COPY_REG(r0);
	COPY_REG(r4);
	COPY_REG(r5);
	COPY_REG(r6);
	COPY_REG(r7);
	COPY_REG(orig_r4);
	COPY_REG(r15_pc);
	COPY_REG(r8);
	COPY_REG(flags);
	COPY_REG(r13_sp);
	COPY_REG(r9);
#undef COPY_REG
}

#endif	/* CALL_CONVENTION_H */
