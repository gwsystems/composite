/*
 * Do NOT include this file. Instead, include cos_regs.h.
 */

#pragma once

#include <cos_environ_constants.h>

#define REG_STATE_PREEMPTED 0 /* The registers must be fully restored as they represent preempted state */
#define REG_STATE_SYSCALL   1 /* The registers don't require full restoration, and can use fastpaths */

/*
 * First, the kernel utilities to store registers, map them between
 * the assembly that saves them, and the structures that organizes
 * them, and the meta-data that tracks their state.
 */

#define REGS_NUM_ARGS_RETS  12	/* # general purpose registers - clobbered registers are part of the syscall (i.e. 2) */
/*
 * On an upcall to user-level, the first three registers are the
 * arguments passed on upcalls (e.g. synchronous invocations):
 */
#define REGS_ARG_COREID     0
#define REGS_ARG_THDID      1
#define REGS_ARG_TOKEN      2 	/* invocation token for invocation, endpoint for sync RPC */
/*
 * On a system call, the first two registers are the capability to
 * invoke, and the operation to perform on it.
 */
#define REGS_ARG_CAP        0
#define REGS_ARG_OPS        1
/*
 * The rest of the registers (all 9 of them) are either arguments or
 * return values. They might be passed to the kernel on a capability
 * invocation that modifies a kernel resource, or they might be passed
 * via IPC between a client and server.
 */
#define REGS_ARGS_RETS_BASE 3   /* Where the general purpose arguments begin */
#define REGS_MAX_NUM_ARGS   9 	/* REGS_NUM_ARGS_RETS - 3 (for sinv: coreid/thdid/token or call: thdid/epid/token) */


/* rflags value during upcalls */
#define REGS_RFLAGS_DEFAULT  0x3200
#define REGS_TRAPFRAME_SZ    0x30
#define REGS_STATE_ERROR_SZ  0x10 /* The size of the state/type plus the error code */
#define REGS_STATE_SZ        0x8

/*
 * Direct assembly requires single %, while inline assembly requires
 * %%. We need these macros to make the following code generic across
 * both.
 */
#ifdef __ASSEMBLER__
#define PREFIX(r) %r
#else  /* __ASSEMBLER__ */
#define PREFIX(r) %%r
#endif  /* __ASSEMBLER__ */

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
 * Note that the last push of the `1` sets the register state so that
 * the calling convention is for a system call (`REG_STATE_SYSCALL`).
 */

/*
 * x86_64 calling conventions pass arguments in `rdi`, `rsi`, `rdx`,
 * `rcx`, `r8`, `r9`, and return value in `rax`. So we want arguments
 * to be naturally placed in registers so that the receiving end of an
 * invocation can easily call the corresponding function.
 */
#define PUSH_REGS_GENERAL	\
	pushq PREFIX(r15);	\
	pushq PREFIX(r14);	\
	pushq PREFIX(r13);	\
	pushq PREFIX(r12);	\
	pushq PREFIX(r10);	\
	pushq PREFIX(r9);	\
	pushq PREFIX(r8);	\
	pushq PREFIX(rdi);	\
	pushq PREFIX(rsi);	\
	pushq PREFIX(rdx);	\
	pushq PREFIX(rbx);	\
	pushq PREFIX(rax)

/*
 * The subtraction makes room for the (irrelevant, unused) trap frame.
 *
 * The push of the `1` populates the `reg_state_t` with a value that
 * specifies we're in a system call.
 *
 * Note that `r11` should hold the rflags value, and the `rcx` holds
 * the instruction pointer to return to. For details, see the
 * `syscall` instruction documentation. The `rbp` register holds stack
 * context for user-level, and should be restored exactly.
 */
#define PUSH_REGS_SYSCALL					\
	subq $(REGS_TRAPFRAME_SZ), PREFIX(rsp);			\
	pushq $(REG_STATE_SYSCALL);				\
	pushq PREFIX(rbp);					\
	pushq PREFIX(r11);					\
	pushq PREFIX(rcx);					\
	PUSH_REGS_GENERAL

/*
 * Push the registers consistent with a trap in which all registers
 * must be saved for future restoration. This happens in both
 * interrupts and exceptions.
 *
 * Not shown here is the *hardware's* saving of the trap-frame.
 *
 * We skip over saving ip/sp as the ip/sp in the trap frame should be
 * used instead, we set the register state to `0` to denote a
 * preempted state, and we avoid saving fs_base/gs_base as those will
 * be taken care of by C code.
 */
#define PUSH_REGS_TRAP 			\
	pushq $(REG_STATE_PREEMPTED);	\
	pushq PREFIX(rbp);		\
	pushq PREFIX(r11);		\
	pushq PREFIX(rcx);		\
	PUSH_REGS_GENERAL

#define POP_REGS_GENERAL	\
	popq PREFIX(rax);	\
	popq PREFIX(rbx);	\
	popq PREFIX(rdx);	\
	popq PREFIX(rsi);	\
	popq PREFIX(rdi);	\
	popq PREFIX(r8);	\
	popq PREFIX(r9);	\
	popq PREFIX(r10);	\
	popq PREFIX(r12);	\
	popq PREFIX(r13);	\
	popq PREFIX(r14);	\
	popq PREFIX(r15)

/*
 * Restore all of the registers. Assumes that clobbered.r11 holds the
 * rflags from the syscall. Does *not* restore the trap frame, which
 * is ignored and left, untouched on the stack.
 *
 * We assume that "cc" is specified in the clobber list of
 * the system call, thus we don't need to restore the specific rflags
 * from when the syscall was made.
 *
 * FIXME: restoring IOPL 3 into rflags. Remove this access.
 */
#define POP_REGS_RET_SYSCALL			\
	POP_REGS_GENERAL;			\
	popq PREFIX(rcx);			\
	popq PREFIX(r11);			\
	popq PREFIX(rsp);			\
	swapgs;					\
	sysretq;

/*
 * Restore registers directly, then (the `add`) skip over the state
 * and error code. Finally, return using iret.
 *
 * Note that the pair of `sti; iretq` are treated as atomic within
 * x86, so there are no preemptions until we return to user-level.
 * Note also that we're adding `0x10` to the stack pointer here (which
 * looks unaligned if you look at `struct regs`) to pop off the
 * `regs_state_t` and the error code.
 *
 * FIXME: remove the sti.
 */
#define POP_REGS_RET_TRAP				\
	POP_REGS_GENERAL;				\
	popq PREFIX(rcx);				\
	popq PREFIX(r11);				\
	popq PREFIX(rbp);				\
	addq $(REGS_STATE_ERROR_SZ), PREFIX(rsp);	\
	swapgs;						\
	sti;						\
	iretq;

/**
 * User-level assembly logic for handling upcalls. When an upcall
 * executes, it must be in assembly, as it does *not yet have a C
 * execution stack*. Additionally, many registers contain arguments
 * and threadid/coreid/token, which cannot be clobbered. As such,
 * the upcall logic needs to:
 *
 * 1. acquire a stack, without clobbering argument registers,
 * 2. populate the `regs_user_context` structure at the top of the
 *    stack that includes the threadid/coreid/token for component
 *    access,
 * 3. setup the arguments and return values, and
 * 4. call the C function associated with the upcall.
 */

/*
 * Used at user-level upon upcall to save the context necessary for
 * execution -- including the coreid (rax), thread id (rbx), and
 * invocation token (rdx). Step 2 above.
 *
 * Should push the invocation token threadid, and then coreid to match
 * the structure of `struct regs_user_context`.
 *
 * This assumes that the `stack_context` has already been placed onto
 * the stack by the stack "getting" code (in `REGS_USER_GET_STACK`).
 */
#define REGS_PUSH_USER_CONTEXT		\
	pushq PREFIX(rdx);		\
	pushq PREFIX(rbx);		\
	pushq PREFIX(rax);

/*
 * REGS_PUSH_USER_ARG6 sets up the system to pass the arguments from
 * the kernel, to the user-level function. This essentially translates
 * the argument layout from how the kernel passed it up to the upcall,
 * into what the x86_64 ABI requires.
 *
 * REGS_POP_USER_ARG6 prepares the return values for a return from
 * synchronous invocation.
 *
 * Assumptions:
 *
 * - PUSH assumes that we have a stack.
 * - All modifications to the stack maintain a 16-byte alignment as
 *   required by the ABI.
 *
 * The calling convention passes arguments 0 through 5 in :
 *
 * rdi, rsi, rdx, rcx, r8, r9
 *
 * Then pass the rest in right-to-left argument order on the stack.
 *
 * Step 3 above.
 *
 * TODO: stack passing of the next three.
 */
#define REGS_USER_SETUP_ARG6		\
	movq PREFIX(rsi), PREFIX(rdx);	\
	movq PREFIX(rdi), PREFIX(rsi);	\
	movq PREFIX(rdx), PREFIX(rdi);	\
	movq PREFIX(r8),  PREFIX(rdx);	\
	movq PREFIX(r9),  PREFIX(rcx);	\
	movq PREFIX(r10), PREFIX(r8);	\
	movq PREFIX(r12), PREFIX(r9);

#define REGS_USER_SETUP_ARG_REST					\
	/* Add stack ctxt entry to maintain proper stack alignment */	\
	pushq $0;							\
	pushq PREFIX(r15);						\
	pushq PREFIX(r14);						\
	pushq PREFIX(r13);

#define REGS_USER_TEARDOWN_ARG6
#define REGS_USER_TEARDOWN_ARG_REST		\
	/* sub-away the 4 arguments  */		\
	add $(8 * 4), PREFIX(rsp);

#define REGS_USER_SETUP_ARGS			\
	REGS_USER_SETUP_ARG6			\
	REGS_USER_SETUP_ARG_REST

#define REGS_USER_TEARDOWN_ARGS			\
	REGS_USER_TEARDOWN_ARG_REST		\
	REGS_USER_TEARDOWN_ARG6

#if COS_STATIC_DIRECT_MAPPED_STACKS

/*
 * REGS_USER_GET_STACK establishes a stack to use for further
 * execution. This is trivial, and just relies on stacks of a fixed
 * size, allocated in an array.
 *
 * This clobbers rcx, and initializes rsp.
 *
 * REGS_USER_PUT_STACK gives up the stack. Nothing to be done for this
 * policy! Assumes that the stack is no longer accessed after this
 * macro is used.
 *
 * Find the structures backing stacks in `struct cos_stack_layout` (below),
 * and the memory for the stacks in `cos_component.h`.
 *
 * TODO: customization of stack allocation policies and size.
 */
#define REGS_USER_GET_STACK                                                     \
	movabs $cos_static_stack, PREFIX(rsp);                                  \
	/* rbx holds the thread id */                                           \
	movq PREFIX(rbx), PREFIX(rcx);                                          \
	/* simple math: this thread's stack offset = stack_size * tid */        \
	shl $COS_STACK_SIZE_ORDER, PREFIX(rcx);					\
	/* add the stack offset to the base to get the current thread's stack*/ \
	add PREFIX(rcx), PREFIX(rsp);

#define REGS_USER_PUT_STACK

#endif	/* COS_STATIC_DIRECT_MAPPED_STACKS */

/*
 * Core upcall logic in user-level. Upon upcall, the following
 * registers are *free* and available for scratch use:
 *
 * rcx, r11, rsp, rbp
 *
 * Further, the core id and thread id are in:
 *
 * - core id = rax
 * - thread id = rbx
 *
 * TODO: return more than one value from sync invs.
 */
#define REGS_USER_UPCALL(asmfn, cfn)				\
.text;								\
.globl asmfn;							\
.extern cfn;							\
.type  asmfn, @function;					\
.align 16;							\
asmfn##:							\
	REGS_USER_GET_STACK					\
	REGS_PUSH_USER_CONTEXT					\
	/* ABI mandate a 16-byte alignment stack pointer*/	\
	and $~0xF, PREFIX(rsp);					\
	REGS_USER_SETUP_ARGS					\
	xor PREFIX(rbp), PREFIX(rbp);				\
	call cfn;						\
	/* return value is in rax, save it if clobbered */	\
	REGS_USER_TEARDOWN_ARGS					\
	REGS_USER_PUT_STACK					\
	/* move the return value to the first argument */	\
	movq PREFIX(rax), PREFIX(rdx);				\
	/* capability 0 = "sinv return" */			\
	xor PREFIX(rax), PREFIX(rax);				\
	syscall;

#ifndef __ASSEMBLER__

#include <cos_compiler.h>
#include <cos_consts.h>
#include <cos_types.h>
#include <cos_consts.h>
#include <cos_types.h>

/***
 * Kernel-level register manipulation functions. Used to define the
 * kernel's register layout, how the calling convention is managed,
 * and how registers are restored. This must consider the cases where
 * registers are saved/restored via a trap, and via a system call, as
 * on x86 these must differ (system call instructions clobber
 * registers, making it impossible to use that path to restore *all*
 * registers after e.g. an interrupt).
 */

#define ASM_SYSCALL_RETURN(rs) \
	asm volatile("movq %0, %%rsp;" EXPAND(POP_REGS_RET_SYSCALL) : : "r" (rs) : "memory" )
#define ASM_TRAP_RETURN(rs) \
	asm volatile("movq %0, %%rsp;" EXPAND(POP_REGS_RET_TRAP) : : "r" (rs) : "memory" )

COS_STATIC_ASSERT(REGS_MAX_NUM_ARGS + REGS_ARGS_RETS_BASE == REGS_NUM_ARGS_RETS,
		  "The relationship between REGS_MAX_NUM_ARGS, REGS_ARGS_RETS_BASE, and REGS_NUM_ARGS_RETS is not correct.");

typedef uword_t        reg_state_t;       /* the state of a register set: preempted, or partially saved  */

COS_STATIC_ASSERT(sizeof(reg_state_t) == REGS_STATE_SZ, "Register state size constant doesn't match size.");

/*
 * The trap frame added onto the stack by interrupts/exceptions. This
 * is only valid/used for traps (interrupts or exceptions), where
 * `regs.state == REG_STATE_PREEMPTED`. Otherwise we have a constant
 * flags value for user-level, and should use the `ip`/`sp` in `struct
 * regs_clobbered`.
 */
struct trap_frame {
	uword_t errcode;
	uword_t ip;
	uword_t cs;
	uword_t flags;
	uword_t sp;
	uword_t ss;
};

/*
 * When a system call is made, these are registers that are clobbered
 * during `syscall` and on returning to user-level with `sysret`. We
 * use these to pass ip/sp from user-level, and to restore ip/flags on
 * return to user-level. When a trap occurs, these are simply normal
 * `rcx`/`r11`/`rbp` registers that must be fully restored.
*/
struct regs_clobbered {
	/*
	 * Used to hold instruction pointer on syscall/sysret, and rcx
	 * on trap
	 */
	uword_t rcx_ip;
        /*
	 * `r11` holds rflags on `syscall`/`sysret`, `r11` on trap.
	 */
	uword_t r11;
	/*
	 * Used to store stack pointer on syscall (which is in rbp via
	 * the system call conventions), and stores `rbp` on trap.
	 */
	uword_t rbp_sp;
};

/*
 * The register set for a thread. This structure exists one
 * per-thread, and once on each core's stack.
 *
 * Four registers are clobbered on return to user-level via the sysret
 * semantics: r11, rcx, rip, and rsp. Additionally `rbp` is used to
 * pass the to-be-restored stack pointer on system call, thus is it
 * also clobbered. These are represented in a combination of
 * `->clobbered` and `frame`.
 *
 * Three registers are devoted to either passing the capability and
 * operations on a system call, or passing coreid, thdid, and
 * invocation token on upcall/return from system call. These are the
 * first three registers in `->args`. The registers used for these
 * are:
 *
 * - rax, rbx, rdx
 *
 * The rest of the (9) registers are used to pass arguments and return
 * values. These remaining registers are generally used to pass data
 * as function arguments or return values. They are ordered as such:
 *
 * - rsi, rdi, r8, r9, r10, r12, r13, r14, r15
 *
 * On a system call (state == `REG_STATE_SYSCALL`), `clobbered.rcx_ip`
 * holds the user-level IP to return to, and `clobbered.r11_sp` holds
 * the stack pointer to return to.
 *
 * On a trap, (state == `REG_STATE_PREEMPTED`), both hold the actual
 * `rcx`, `rbp`, and `r11` that must be properly restored. The `ip`
 * and `sp` are stored in the `struct trap_frame`.
 *
 * To access/manipulate the registers within the kernel, use:
 *
 * - `regs_arg`
 * - `regs_retval`
 * - `regs_cap_op`
 * - `regs_set_upcall_args`
 * - `regs_ip_sp`
 * - `regs_set_ip_sp`
 * - `regs_preempted`
 *
 * From user-level, use the `cos_syscall_*_*` functions.
 */
struct regs {
	uword_t args[REGS_NUM_ARGS_RETS]; /* arguments/retvals for syscalls */
	struct regs_clobbered clobbered;
	reg_state_t state;	/* `0` = trap, `1` = syscall */
	struct trap_frame frame;
};

COS_STATIC_ASSERT((sizeof(struct regs) - (sizeof(reg_state_t) + sizeof(struct trap_frame))) / sizeof(long) == 15,
		  "struct regs size doesn't match the number of general purpose registers.");
COS_STATIC_ASSERT(sizeof(struct trap_frame) == REGS_TRAPFRAME_SZ,
		  "Trap frame register structure of incorrect size.");

/*
 * Upon an upcall into a component (at user-level), these values are
 * saved on the stack. Thus, this structure defines an interface
 * between the values of the registers upon upcall, and the C code
 * that accesses these values.
 *
 * The rest of the arguments are passed to the destination C function
 * using the architecture's calling conventions (i.e. in registers, on
 * the stack, etc...). The best documentation includes:
 *
 * - https://www.agner.org/optimize/calling_conventions.pdf
 * - https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf
 *
 * We lay this out to directly write the registers onto the stack as
 * `uword_t`s (even though the actual types are smaller). This
 * optimizes for the fast-path of setting up the context, at a slight
 * cost for access.
 *
 * The last item (`stack_context`) can be used by the stack management
 * code to track the meta-data necessary. This is mainly used in
 * `REGS_USER_GET/PUT_STACK`.
 */
struct regs_user_context {
	uword_t coreid;
	uword_t thdid;
	uword_t inv_token;
	uword_t stack_context;	/* Can be used by the stack tracking code */
};

#define REGS_USER_CONTEXT_SZ 32

COS_STATIC_ASSERT(sizeof(struct regs_user_context) == REGS_USER_CONTEXT_SZ,
		  "The user register context struct size doesn't match REGS_USER_CONTEXT_SZ.");

static inline coreid_t
regs_user_context_coreid(struct regs_user_context *c)
{ return c->coreid; }

static inline id_token_t
regs_user_context_thdid(struct regs_user_context *c)
{ return c->thdid; }

static inline inv_token_t
regs_user_context_invtoken(struct regs_user_context *c)
{ return c->inv_token; }

/*
 * This is the layout for a user-level execution stack. Whenever we
 * upcall into a component (i.e. for a new thread, or due to a
 * synchronous invocation). A static stack has an overflow detection
 * word (that should always be 0), the actual stack to memory to be
 * used as the runtime stack, and, finally, the context placed on the
 * stack upon upcall which includes the thread id, core id, and
 * invocation token.
 */
struct cos_stack_layout {
	uword_t overflow_detection;
	u8_t stack[COS_STACK_SIZE - sizeof(uword_t) - sizeof(struct regs_user_context)];
	struct regs_user_context context;
} __attribute__((packed));
COS_STATIC_ASSERT(((sizeof(uword_t) + COS_STACK_SIZE - sizeof(uword_t) - sizeof(struct regs_user_context)) % sizeof(uword_t)) == 0,
                  "The stack context in cos_stack_layout is not aligned on a word boundary.");

/*
 * Provides the format string, and arguments for printing out a register set.
 */
#define COS_REGS_PRINT_ARGS(r)                                                                                     \
	"%s registers - ip: %lx, sp: %lx\n"				                                           \
	"bp: %lx, a: %lx, b: %lx, c: %lx, d: %lx, si: %lx, di: %lx\n"                                              \
	"8: %lx, 9: %lx, 10: %lx, 11: %lx, 12: %lx, 13: %lx, 14: %lx, 15: %lx\n"                                   \
	"cs: %lx, ss: %lx, flags: %lx\n",									   \
	(r->frame.cs & 3) ? "User" : "Kernel", (r->state == REG_STATE_SYSCALL) ? r->clobbered.rcx_ip : r->frame.ip,\
	(r->state == REG_STATE_SYSCALL ? r->clobbered.rbp_sp : r->frame.sp), r->clobbered.rbp_sp, r->args[0],      \
	r->args[1], r->clobbered.rcx_ip, r->args[2], r->args[3], r->args[4], r->args[5], r->args[6], r->args[7],   \
	r->clobbered.r11, r->args[8], r->args[9], r->args[10], r->args[11], r->frame.cs, r->frame.ss, r->frame.flags

/*
 * `userlevel_eager_return_syscall` returns from kernel back to
 * user-level immediately, under the assumption that the registers
 * have `state == 1`, thus we can return using the sysret fastpath.
 */
COS_FASTPATH COS_NO_RETURN static inline void
userlevel_eager_return_syscall(struct regs *rs)
{
	ASM_SYSCALL_RETURN(rs);

	while (1) ;
}

/*
 * `userlevel_eager_return` immediately returns from kernel to
 * user-level using the syscall or trap logic, depending on the format
 * of the registers.
 */
COS_FASTPATH COS_NO_RETURN static inline void
userlevel_eager_return(struct regs *rs)
{
	if (likely(rs->state == REG_STATE_SYSCALL)) ASM_SYSCALL_RETURN(rs);
	else if (rs->state == REG_STATE_PREEMPTED)  ASM_TRAP_RETURN(rs);

	while (1) ;
}

struct fpu_regs {
	u16_t cwd; /* Control Word */
	u16_t swd; /* Status Word */
	u16_t twd; /* Tag Word */
	u16_t fop; /* Last Instruction Opcode */
	union {
		struct {
			u64_t rip; /* Instruction Pointer */
			u64_t rdp; /* Data Pointer */
		};
		struct {
			u32_t fip; /* FPU IP Offset */
			u32_t fcs; /* FPU IP Selector */
			u32_t foo; /* FPU Operand Offset */
			u32_t fos; /* FPU Operand Selector */
		};
	};
	u32_t mxcsr;      /* MXCSR Register State */
	u32_t mxcsr_mask; /* MXCSR Mask */

	/* 8*16 bytes for each FP-reg = 128 bytes: */
	u32_t st_space[32];

	/* 16*16 bytes for each XMM-reg = 256 bytes: */
	u32_t xmm_space[64];

	u32_t padding[12];

	union {
		u32_t padding1[12];
		u32_t sw_reserved[12];
	};
	/* Above is lagecy 512 bytes area */

	union {
		struct {
			/* XSAVE Header, followed by reserved area */
			u64_t xstate_bv;
			u64_t xcomp_bv;
		};
		u8_t header_area[64];
	};

	/* Offset here should be at 576 bytes */

	/*
	 * 800 is calculated by sizeof(struct thread) - 576,
	 * with a little reserved area left in struct thread. This
	 * is to make sure all members of struct thread is still
	 * in a single page. This should be OK because the size
	 * is big enough to save all SSE and AVX2 state components.
	 *
	 * Note this will not work with AVX512! You have to make sure
	 * closing to save AVX512 component in XCR0.
	 */
	u8_t xsave_ext_area[800];
} __attribute__((aligned(64)));

/**
 * `regs_arg` retrieves a thread's argument. Assumptions:
 *
 * - `!regs_preempted(rs)`
 * - `argno < REGS_MAX_NUM_ARGS`
 */
COS_FORCE_INLINE static inline uword_t
regs_arg(struct regs *rs, unsigned int argno)
{
	return rs->args[REGS_ARGS_RETS_BASE + argno];
}
COS_STATIC_ASSERT(REGS_ARGS_RETS_BASE == 3, "RETS base redefined?");

/**
 * `regs_cap_op` retrieves a system call's capability and operations
 * arguments.
 *
 * Assumes that `!regs_preempted(rs)`.
 */
COS_FORCE_INLINE static inline void
regs_cap_op(struct regs *rs, cos_cap_t *cap, cos_op_bitmap_t *ops)
{
	*cap = rs->args[REGS_ARG_CAP];
	*ops = rs->args[REGS_ARG_OPS];
}

/**
 * `regs_ip_sp` returns the thread's instruction and stack pointers.
 *
 * Assumes that `!regs_preempted(rs)`.
 */
COS_FORCE_INLINE static inline void
regs_ip_sp(struct regs *rs, uword_t *ip, uword_t *sp)
{
	*ip = rs->clobbered.rcx_ip;
	*sp = rs->clobbered.rbp_sp;
}

/**
 * `regs_retval` sets the return value `retvalno` for a thread.
 *
 * Assumes:
 *
 * - `!regs_preempted(rs)`
 * - `retvalno < REGS_MAX_NUM_ARGS`
 */
COS_FORCE_INLINE static inline void
regs_retval(struct regs *rs, unsigned int retvalno, uword_t val)
{
	rs->args[REGS_ARGS_RETS_BASE + retvalno] = val;
}

/**
 * `regs_set_upcall_args` sets the register values necessary for an
 * upcall.
 *
 * Assumes that `!regs_preempted(rs)`.
 */
COS_FORCE_INLINE static inline void
regs_set_upcall_args(struct regs *rs, coreid_t coreid, id_token_t thdid, inv_token_t tok)
{
	rs->args[REGS_ARG_COREID] = coreid;
	rs->args[REGS_ARG_THDID]  = thdid;
	rs->args[REGS_ARG_TOKEN]  = tok;
}

/**
 * `regs_set_ip_sp` sets the register set's instruction and stack
 * pointers.
 *
 * Assumes that `!regs_preempted(rs)`.
 */
COS_FORCE_INLINE static inline void
regs_set_ip_sp(struct regs *rs, uword_t ip, uword_t sp)
{
	rs->clobbered.rcx_ip = ip;
	rs->clobbered.rbp_sp = sp;
}

/**
 * `regs_preempted` returns `1` if the registers represent a preempted
 * state, and `0` if instead it represents a cooperatively saved
 * register state (i.e. due to a system call).
 */
COS_FORCE_INLINE static inline int
regs_preempted(struct regs *rs)
{
	return rs->state == REG_STATE_PREEMPTED;
}

/**
 * `regs_prepare_upcall` sets up the register set with the proper
 * setup for an upcall. That is to say, we will create execution at a
 * specific instruction pointer in the thread's component, rather than
 * continue at a previous instruction pointer. This is used for thread
 * migration-based upcalling into the server, and for initial thread
 * execution when creating the thread. The setup includes passing the
 * core and thread ids, and a token to be used by the assembly stubs
 * in the component. The token is only used for synchronous
 * invocations which must pass client-identifying information in the
 * token.
 *
 * Assumes that `rs->state == REG_STATE_SYSCALL` as it makes no sense
 * to override the registers otherwise.
 */
COS_FORCE_INLINE static inline void
regs_prepare_upcall(struct regs *rs, vaddr_t entry_ip, coreid_t coreid, id_token_t thdid, inv_token_t tok)
{
	regs_set_upcall_args(rs, coreid, thdid, tok);
	rs->clobbered = (struct regs_clobbered) {
		.rcx_ip = entry_ip,
		.r11    = REGS_RFLAGS_DEFAULT,
		.rbp_sp = 0 	/* user-level establishes its own stack */
	};

	return;
}

/***
 * The user-level system call and register manipulation implementation
 * follows. This includes system call functions, and upcall handling
 * code.
 */

/*
 * We use this frame ctx to save bp(frame pointer) and sp(stack
 * pointer) before it goes into the kernel through syscall.
 *
 * We cannot use a push %rbp/%rsp because the compiler doesn't know
 * the inline assembly code changes the stack pointer. Thus, in some
 * cases the compiler might do some optimizations which could conflict
 * with the push %rbp/%rsp. That means, the push might corrupt some
 * stack local variables. Thus, instead of using push instruction, we
 * use mov instruction to first save them to a stack local
 * position(regs_frame_ctx) and then pass the address of the frame_ctx
 * to the kernel rather than the sp. When the syscall returns, the
 * kernel will restore the sp to the address of regs_frame_ctx, thus
 * we can simply use two pops to restore the original bp and sp.
 */
struct regs_frame_ctx {
	uword_t bp;
	uword_t sp;
};

#define REGS_CTXT_BP_OFF 0
#define REGS_CTXT_SP_OFF 8

COS_STATIC_ASSERT(REGS_CTXT_SP_OFF == offsetof(struct regs_frame_ctx, sp),
		  "Offset for stack pointer in system call context is inconsistent");
COS_STATIC_ASSERT(REGS_CTXT_BP_OFF == offsetof(struct regs_frame_ctx, bp),
		  "Offset for base pointer in system call context is inconsistent");

#define REGS_SYSCALL_CTXT						\
	struct regs_frame_ctx frame_ctx;				\
	register uword_t r11_ctx __asm__("r11") = (uword_t)&frame_ctx

/*
 * The `syscall` instruction saves the instruction pointer after the
 * `syscall` into `rcx`, and the `rflags` into `r11`. This assembly
 * assumes that incoming
 *
 * 1. `rcx` holds a pointer to the `struct regs_frame_ctx` we'll use
 *    to save sp/bp,
 * 2. that we cannot include `rbp` in any of the inline-assembly
 *    input/output/clobber lists, so we need to manually save/restore
 *    that.
 */
#define REGS_SYSCALL_TEMPLATE				\
	"movq %%rbp, " EXPAND(REGS_CTXT_BP_OFF) "(%%r11)\n\t"	\
	"movq %%rsp, " EXPAND(REGS_CTXT_SP_OFF) "(%%r11)\n\t"	\
							\
	"movq %%r11, %%rbp\n\t"				\
	"syscall\n\t"					\
	"popq %%rbp\n\t"				\
	"popq %%rsp\n\t"

/*
 * This is painful, and I wish that there were a better way to feed
 * registers into inline assembly in x86_64. Instead, if musl does
 * this way, we'll do it this way.
 * https://git.musl-libc.org/cgit/musl/tree/arch/x86_64/syscall_arch.h
 */

/* Rest of the args are in the input inline asm list */
#define REGS_SYSCALL_DECL				\
	register uword_t r8  __asm__("r8")  = 0;	\
	register uword_t r9  __asm__("r9")  = 0;	\
	register uword_t r10 __asm__("r10") = 0;	\
	register uword_t r12 __asm__("r12") = 0;	\
	register uword_t r13 __asm__("r13") = 0;	\
	register uword_t r14 __asm__("r14") = 0;	\
	register uword_t r15 __asm__("r15") = 0

#define REGS_SYSCALL_DECL_ARGS4			\
	REGS_SYSCALL_DECL;			\
	r8 = a2;				\
	r9 = a3

#define REGS_SYSCALL_DECL_ARGS9				\
	REGS_SYSCALL_DECL_ARGS4;			\
	r10 = a4;					\
	r12 = a5;					\
	r13 = a6;					\
	r14 = a7;					\
	r15 = a8

#define REGS_SYSCALL_DECL_POSTRETS4		\
	*ret2 = r8;				\
        *ret3 = r9

#define REGS_SYSCALL_DECL_POSTRETS9			\
	REGS_SYSCALL_DECL_POSTRETS4;			\
	*ret4 = r10;					\
	*ret5 = r12;					\
	*ret6 = r13;					\
	*ret7 = r14;					\
	*ret8 = r15

/*
 * Here nothing is explicitly returned in `r11_ctx`, but the inline
 * assembly has to convey that the `r11` register has been clobbered.
 * This is the prescribed way.
 */
#define REGS_SYSCALL_ARG4						\
	"r" (r11_ctx), "a" (cap), "b" (ops), "d" (0), "S" (a0), "D" (a1), "r" (r8), "r" (r9)
#define REGS_SYSCALL_ARG9						\
	REGS_SYSCALL_ARG4, "r" (r10), "r" (r12), "r" (r13), "r" (r14), "r" (r15)

#define REGS_SYSCALL_RET						\
	"=r" (r11_ctx), "=S" (*ret0), "=D" (*ret1), "=r" (r8), "=r" (r9), "=r" (r10), "=r" (r12), "=r" (r13), "=r" (r14), "=r" (r15)

#define REGS_SYSCALL_CLOBBER		\
	"memory", "cc", "%rcx"

#define REGS_SYSCALL_FN_ARGS4 uword_t a0, uword_t a1, uword_t a2, uword_t a3
#define REGS_SYSCALL_FN_ARGS9 REGS_SYSCALL_FN_ARGS4, uword_t a4, uword_t a5, uword_t a6, uword_t a7, uword_t a8
#define REGS_SYSCALL_FN_RETS4 uword_t *ret0, uword_t *ret1, uword_t *ret2, uword_t *ret3
#define REGS_SYSCALL_FN_RETS9 REGS_SYSCALL_FN_RETS4, uword_t *ret4, uword_t *ret5, uword_t *ret6, uword_t *ret7, uword_t *ret8
#define REGS_SYSCALL_FN_4RETS_EMU				\
	uword_t rmem1, rmem2, rmem3, *ret1 = &rmem1, *ret2 = &rmem2, *ret3 = &rmem3

/**
 * The `regs_syscall_x_y` functions make a system call, passing `x`
 * arguments, and returning `y` values. They are designed to use the
 * macros above, enabling register calling conventions to be modified
 * more easily.
 *
 * - `@cap` - capability to activate
 * - `@ops` - the operation(s) to perform on that capability's
 *   resource; note that each capability allows only a subset of
 *   operations
 * - `@a?` - the ?th argument
 * - `@ret?` - the ?th return value
 */

COS_FORCE_INLINE static inline void
cos_syscall_4_4(cos_cap_t cap, cos_op_bitmap_t ops, REGS_SYSCALL_FN_ARGS4, REGS_SYSCALL_FN_RETS4) {
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS4;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET
		     : REGS_SYSCALL_ARG4
		     : REGS_SYSCALL_CLOBBER);
	REGS_SYSCALL_DECL_POSTRETS4;
}

COS_FORCE_INLINE static inline void
cos_syscall_4_1(cos_cap_t cap, cos_op_bitmap_t ops, REGS_SYSCALL_FN_ARGS4, uword_t *ret0) {
	REGS_SYSCALL_FN_4RETS_EMU;
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS4;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET
		     : REGS_SYSCALL_ARG4
		     : REGS_SYSCALL_CLOBBER);
	REGS_SYSCALL_DECL_POSTRETS4;
}

COS_FORCE_INLINE static inline void
cos_syscall_9_1(cos_cap_t cap, cos_op_bitmap_t ops, REGS_SYSCALL_FN_ARGS9, uword_t *ret0) {
	REGS_SYSCALL_FN_4RETS_EMU;
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS9;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET
		     : REGS_SYSCALL_ARG9
		     : REGS_SYSCALL_CLOBBER);
	REGS_SYSCALL_DECL_POSTRETS4;
}

COS_FORCE_INLINE static inline void
cos_syscall_9_4(cos_cap_t cap, cos_op_bitmap_t ops, REGS_SYSCALL_FN_ARGS9, REGS_SYSCALL_FN_RETS4)
{
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS9;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET
		     : REGS_SYSCALL_ARG9
		     : REGS_SYSCALL_CLOBBER);
	REGS_SYSCALL_DECL_POSTRETS4;
}

COS_FORCE_INLINE static inline void
cos_syscall_9_9(cos_cap_t cap, cos_op_bitmap_t ops, REGS_SYSCALL_FN_ARGS9, REGS_SYSCALL_FN_RETS9)
{
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS9;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET
		     : REGS_SYSCALL_ARG9
		     : REGS_SYSCALL_CLOBBER);
	REGS_SYSCALL_DECL_POSTRETS9;
}

#endif	/* !__ASSEMBLER__ */
