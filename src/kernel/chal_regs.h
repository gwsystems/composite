#pragma once

#ifdef __ASSEMBLER__
/* Direct assembly requires single % */
#define PREFIX(r) %r
#else
/* Inline assembly requires double % */
#define PREFIX(r) %%r

#define STRINGIFY(...) #__VA_ARGS__
#define EXPAND(X) STRINGIFY(X)

#define ASM_SYSCALL_RETURN(rs) \
	asm volatile("lea %0, %%rsp;" EXPAND(POP_REGS_RET_SYSCALL) : : "m" (&rs[1]) : "memory" )
#define ASM_TRAP_RETURN(rs) \
	asm volatile("lea %0, %%rsp;" EXPAND(POP_REGS_RET_TRAP) : : "m" (&rs[1]) : "memory" )

#endif

/***
 * Save and restore the registers as appropriate for a system call.
 * The stack layout is defined by `struct regs`.
 *
 * We keep rcx and r11 separated from the other registers as we'll use
 * the other registers to directly access arguments/retvals by index,
 * and these two registers are clobbered as part of the sysret
 * (return to user) procedure. Thus we don't want them to be
 * included in "return values to user-level" consideration.
 *
 * The first subtract by 0x8 makes room for the trap frame, and the
 * fs/gs base.
 *
 * The first two pushes of %r11 and %rcx are for the stack and
 * instruction pointers, and the next two save `0`s into the %rcx and
 * %r11 slots in the structures.
 *
 * Note that the last push of the `1` sets the register state so
 * that the calling convention is for a system call.
 */
#define PUSH_REGS_GENERAL	\
	pushq PREFIX(r15);	\
	pushq PREFIX(r14);	\
	pushq PREFIX(r13);	\
	pushq PREFIX(r12);	\
	pushq PREFIX(r10);	\
	pushq PREFIX(r9);	\
	pushq PREFIX(r8);	\
	pushq PREFIX(rbp);	\
	pushq PREFIX(rdi);	\
	pushq PREFIX(rsi);	\
	pushq PREFIX(rdx);	\
	pushq PREFIX(rbx);	\
	pushq PREFIX(rax);	\

#define PUSH_REGS_SYSCALL		\
	addq $0x40, PREFIX(rsp);	\
	pushq $1;			\
	pushq PREFIX(r11);		\
	pushq PREFIX(rcx);		\
	PUSH_REGS_GENERAL		\

/*
 * Push the registers consistent with a trap in which all registers
 * must be saved for future restoration. This happens in both
 * interrupts and exceptions.
 *
 * We skip over saving ip/sp as the ip/sp in the trap frame should be
 * used instead, we set the register state to `0` to denote a
 * preempted state, and we avoid saving fs_base/gs_base as those will
 * be taken care of by C code.
 */
#define PUSH_REGS_TRAP 			\
	pushq $0;			\
	pushq PREFIX(r11);		\
	pushq PREFIX(rcx);		\
	PUSH_REGS_GENERAL		\

#define POP_REGS_GENERAL	\
	popq PREFIX(rax);	\
	popq PREFIX(rbx);	\
	popq PREFIX(rdx);	\
	popq PREFIX(rsi);	\
	popq PREFIX(rdi);	\
	popq PREFIX(rbp);	\
	popq PREFIX(r8);	\
	popq PREFIX(r9);	\
	popq PREFIX(r10);	\
	popq PREFIX(r12);	\
	popq PREFIX(r13);	\
	popq PREFIX(r14);	\
	popq PREFIX(r15);	\

/*
 * This is not obvious. We're putting the rflags value (the magic
 * constant) into rflags, and then we're popping the
 * `clobbered.r11_sp` value into the stack pointer. Thus, after
 * using this macro, the stack should no longer be accessed.
 *
 * FIXME: restoring IOPL 3 into rflags. Remove this access.
 */
#define POP_REGS_RET_SYSCALL    	\
	POP_REGS_GENERAL		\
	popq PREFIX(rcx);		\
	movq $0x3200, PREFIX(r11);	\
	popq PREFIX(rsp);		\
	swapgs;				\
	sysretq;

/*
 * Restore registers directly, then skip over the state and error
 * code. Finally, return using iret.
 *
 * Note that the pair of `sti; iretq` are treated as atomic within
 * x86, so there are no preemptions until we return to user-level.
 * Note also that we're adding `0x10` to the stack pointer here (which
 * looks unaligned if you look at `struct regs`) to pop off the
 * `regs_state_t` and the error code.
 *
 * FIXME: remove the sti?
 */
#define POP_REGS_RET_TRAP		\
	POP_REGS_GENERAL; 		\
	popq PREFIX(rcx);		\
	popq PREFIX(r11);		\
	addq $0x10, PREFIX(rsp);	\
	swapgs;				\
	sti;				\
	iretq;				\
