#ifndef CALL_CONVENTION_H
#define CALL_CONVENTION_H

/* 
 * Functions to maintain calling conventions on invocation and return
 * (i.e. to make sure the registers are appropriately set up).
 */
static inline void 
__userregs_setret(struct pt_regs *regs, unsigned long ret)
{ regs->ax = ret; }
static inline unsigned long 
__userregs_getsp(struct pt_regs *regs)
{ return regs->bp; }
static inline unsigned long 
__userregs_getip(struct pt_regs *regs)
{ return regs->cx; }
static inline capid_t 
__userregs_getcap(struct pt_regs *regs)
{ return regs->ax & ((1<<27)-1); } /* FIXME: add the shift here */
static inline syscall_op_t 
__userregs_getop(struct pt_regs *regs)
{ return regs->ax >> 27; }
static inline unsigned long 
__userregs_getinvret(struct pt_regs *regs)
{ return regs->cx; } /* cx holds the return value on invocation return path. */
static inline void
__userregs_set(struct pt_regs *regs, unsigned long ret, unsigned long sp, unsigned long ip)
{
	regs->ax = ret;
	regs->sp = regs->cx = sp;
	regs->ip = regs->dx = ip;
}
static inline void
__userregs_sinvupdate(struct pt_regs *regs)
{
	/* IPC calling side has 4 args (in order): bx, si, di, dx */
	/* IPC server side receives 4 args: bx, si, di, bp */
	/* So we need to pass the 4th argument. */

	/* regs->bx = regs->bx; */
	/* regs->si = regs->si; */
	/* regs->di = regs->di; */
	regs->bp = regs->dx;
}
static inline int
__userregs_get1(struct pt_regs *regs)
{ return regs->bx; }
static inline int
__userregs_get2(struct pt_regs *regs)
{ return regs->si; }
static inline int
__userregs_get3(struct pt_regs *regs)
{ return regs->di; }
static inline int
__userregs_get4(struct pt_regs *regs)
{ return regs->dx; }

#endif	/* CALL_CONVENTION_H */
