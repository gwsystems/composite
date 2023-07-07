#pragma once

/***
 * First, the kernel utilities to store registers, map them between
 * the assembly that saves them, and the structures that organizes
 * them, and the meta-data that tracks their state.
 */

#define REG_STATE_PREEMPTED 0 /* The registers must be fully restored as they represent preempted state */
#define REG_STATE_SYSCALL   1 /* The registers don't require full restoration, and can use `sysret` */

#define REGS_NUM_ARGS_RETS  12	/* # general purpose registers - clobbered registers are part of the syscall (i.e. 2) */
#define REGS_RETVAL_BASE    0
#define REGS_MAX_NUM_ARGS   9 	/* REGS_NUM_ARGS_RETS - 3 (for sinv: coreid/thdid/token or call: thdid/epid/token) */
#define REGS_GEN_ARGS_BASE  3	/* where the general purpose arguments begin */

/* Arguments to capability activations/system calls */
#define REGS_ARG_CAP        0
#define REGS_ARG_OPS        1
/* Arguments passed on upcalls (e.g. synchronous invocations) */
#define REGS_ARG_COREID     0
#define REGS_ARG_THDID      1
#define REGS_ARG_TOKEN      2

#define REGS_RFLAGS_DEFAULT  0x3200
#define REGS_TRAPFRAME_SZ    0x30
#define REGS_STATE_ERROR_SZ  0x10 /* The size of the state/type plus the error code */
#define REGS_STATE_SZ        0x8

/*
 * Direct assembly requires single %, while inline assembly requires
 * %%.
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
	pushq PREFIX(rdi);	\
	pushq PREFIX(rsi);	\
	pushq PREFIX(rdx);	\
	pushq PREFIX(rbx);	\
	pushq PREFIX(rax)

/*
 * The subtraction makes room for the (irrelevant, unused) trap frame.
 *
 * The push of the `1` populates the `reg_state_t` with a value that
 * specifies we're in a system call, and the `REGS_RFLAGS_DEFAULT`
 * value populates `r11` on return with a fixed RFLAGS value.
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
	POP_REGS_GENERAL			\
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

#ifndef __ASSEMBLER__

#include <compiler.h>
#include <cos_consts.h>
#include <cos_types.h>
#include <chal_consts.h>
#include <chal_types.h>

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

COS_STATIC_ASSERT(REGS_MAX_NUM_ARGS + REGS_GEN_ARGS_BASE == REGS_NUM_ARGS_RETS,
		  "The relationship between REGS_MAX_NUM_ARGS, REGS_GEN_ARGS_BASE, and REGS_NUM_ARGS_RETS is not correct.");

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
 * The `args` are generally used to pass data as function arguments or
 * return values. They are ordered as such:
 *
 * rax, rbx, rdx, rsi, rdi, r8, r9, r10, r12, r13, r14, r15
 *
 * Note the rcx and r11 are clobbered as part of `syscall`/`sysret`,
 * thus follow the other GP regs in `struct regs_clobbered`.
 * Additionally `rbp` is used to pass the stack pointer on system
 * call, thus is also clobbered.
 *
 * On a system call (state == `1`), `clobbered.rcx_ip` holds the
 * user-level IP to return to, and `clobbered.r11_sp` holds the stack
 * pointer to return to.
 *
 * On a trap, (state == `0`), both hold the actual `rcx` and `r11`
 * that must be properly restored. The `ip` and `sp` are stored in the
 * `struct trap_frame`.
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
 * `userlevel_eager_return_syscall` returns back to user-level
 * immediately, under the assumption that the registers have `state ==
 * 1`, thus we can return using the sysret fastpath.
 */
COS_FASTPATH COS_NO_RETURN static inline void
userlevel_eager_return_syscall(struct regs *rs)
{
	struct regs *end_of_struct = &(rs[1]);

	ASM_SYSCALL_RETURN(end_of_struct);

	while (1) ;
}

/*
 * `userlevel_eager_return` immediately returns to user-level using
 * the syscall or trap logic, depending on the format of the
 * registers.
 */
COS_FASTPATH COS_NO_RETURN static inline void
userlevel_eager_return(struct regs *rs)
{
	struct regs *end_of_struct = &(rs[1]);

	if (rs->state == 0)      ASM_TRAP_RETURN(end_of_struct);
	else if (rs->state == 1) ASM_SYSCALL_RETURN(end_of_struct);

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

	int status;
} __attribute__((aligned(64)));

/**
 * `regs_arg` retrieves a thread's argument. This should only be used
 * when the register's state (`rs->state`) is `REG_STATE_SYSCALL`.
 *
 * Conventions for these arguments when we're making a typical system
 * call include:
 *
 * - `0`: the capability activated with the system call
 * - `1` - `REGS_NUM_ARGS_RETS`: Activation arguments
 *
 * Conventions for returning from a synchronous invocation or
 * calling `reply_and_wait` include:
 *
 * - `0` - normal function return value
 * - `1` - `REGS_NUM_ARGS_RETS` - additional return values
 */
COS_FORCE_INLINE static inline uword_t
regs_arg(struct regs *rs, int argno)
{
	return rs->args[argno];
}

/**
 * `regs_retval` sets the return value `retvalno` for a thread. This
 * should only be used when the register's state (`rs->state`) is
 * `REG_STATE_SYSCALL`.
 *
 * Conventions for these return values for a normal system call
 * include:
 *
 * - `0`: function return value
 * - `1` - `REGS_NUM_ARGS_RETS`: additional return values, where appropriate
 *
 * Conventions for these return values for an *upcall* corresponding
 * to a synchronous invocation into a server component include:
 *
 * - `0`: core id
 * - `1`: thread id
 * - `2`: the synchronous invocation token
 * - `3` - `REGS_NUM_ARGS_RETS`: the function arguments, from 3 to REGS_NUM_ARGS_RETSth.
 *
 * Conventions for these return values for a `call`-based activation
 * of a thread through IPC include:
 *
 * - `0`: core id
 * - `1`: thread id
 * - `2`: the end-point token
 * - `3`: the client identifier token
 * - `4` - `REGS_NUM_ARGS_RETS`: the function arguments, from 4 to REGS_NUM_ARGS_RETSth.
 */
COS_FORCE_INLINE static inline uword_t
regs_retval(struct regs *rs, int retvalno, uword_t val)
{
	return rs->args[retvalno] = val;
}

/**
 * `regs_ip_sp` simply returns the thread's instruction and stack
 * pointers. Assumes that this should only be used if
 * `regs_preempted(...) == 0`.
 */
COS_FORCE_INLINE static inline void
regs_ip_sp(struct regs *rs, uword_t *ip, uword_t *sp)
{
	*ip = rs->clobbered.rcx_ip;
	*sp = rs->clobbered.rbp_sp;
}

/**
 * `regs_set_ip_sp` sets the register set's instruction and stack
 * pointers. Assumes that this should only be used if
 * `regs_preempted(...) == 0`.
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
 * Assumes that `rs->state == REG_STATE_SYSCALL` as it makes not sense
 * to override the registers otherwise.
 */
COS_FORCE_INLINE static inline void
regs_prepare_upcall(struct regs *rs, vaddr_t entry_ip, coreid_t coreid, thdid_t thdid, inv_token_t tok)
{
	rs->args[0] = coreid;
	rs->args[1] = thdid;
	rs->args[2] = tok;
	rs->clobbered = (struct regs_clobbered) {
		.rcx_ip = entry_ip,
		.r11    = 0,
		.rbp_sp = 0
	};

	return;
}

/**
 * `regs_prepare_ipc_retvals` populates the registers to return from a
 * `reply_and_wait` operation in a server thread. These include the
 * thread id that is doing the `call`, the end-point identifier, and
 * the invocation token associated with it.
 *
 * Assumes that `rs->state == REG_STATE_SYSCALL` as it makes not sense
 * to override the registers otherwise.
 */
COS_FORCE_INLINE static inline void
regs_prepare_ipc_retvals(struct regs *rs, thdid_t thdid, id_token_t ep_id, inv_token_t tok)
{
	rs->args[0] = ep_id;
	rs->args[1] = thdid;
	rs->args[2] = tok;

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
 * position(frame_ctx) and then pass the address of the frame_ctx to
 * the kernel rather than the sp. When the syscall returns, the kernel
 * will restore the sp to the address of frame_ctx, thus we can simply
 * use two pops to restore the original bp and sp.
 */
struct frame_ctx {
	uword_t bp;
	uword_t sp;
};

#define REGS_CTXT_BP_OFF 0
#define REGS_CTXT_SP_OFF 8

COS_STATIC_ASSERT(REGS_CTXT_SP_OFF == offsetof(struct frame_ctx, sp),
		  "Offset for stack pointer in system call context is inconsistent");
COS_STATIC_ASSERT(REGS_CTXT_BP_OFF == offsetof(struct frame_ctx, bp),
		  "Offset for base pointer in system call context is inconsistent");

#define REGS_SYSCALL_CTXT						\
	struct frame_ctx frame_ctx;					\
	register uword_t r11_ctx __asm__("r11") = (uword_t)&frame_ctx

/*
 * The `syscall` instruction saves the instruction pointer after the
 * `syscall` into `r11`, and the `rflags` into `r11`. This assembly
 * assumes that incoming 1. `rcx` holds a pointer to the `struct
 * frame_ctxt` we'll use to save sp/bp, 2. that we cannot include
 * `rbp` in any of the inline-assembly input/output/clobber lists, so
 * we need to manually save/restore that.
 */
#define REGS_SYSCALL_TEMPLATE				\
	"movq %%rbp, REGS_CTXT_BP_OFF(%%r11)\n\t"	\
	"movq %%rsp, REGS_CTXT_SP_OFF(%%r11)\n\t"	\
							\
	"movq %%r11, %%rbp\n\t"				\
	"syscall\n\t"					\
	"movq %%rbp, %%rsp\n\t"				\
							\
	"popq %%rbp\n\t"				\
	"popq %%rsp\n\t"

/*
 * This is painful, and I wish that there were a better way to feed
 * registers into inline assembly in x86_64. Instead, if musl does
 * this way, we'll do it this way.
 * https://git.musl-libc.org/cgit/musl/tree/arch/x86_64/syscall_arch.h
 */
#define REGS_SYSCALL_DECL_ARGS4				\
	register uword_t r8 __asm__("r8")   = a3

#define REGS_SYSCALL_DECL_ARGS10			\
	REGS_SYSCALL_DECL_ARGS4;			\
	register uword_t r9  __asm__("r9")  = a4;	\
	register uword_t r10 __asm__("r10") = a5;	\
	register uword_t r12 __asm__("r12") = a6;	\
	register uword_t r13 __asm__("r13") = a7;	\
	register uword_t r14 __asm__("r14") = a8;	\
	register uword_t r15 __asm__("r15") = a9

#define REGS_SYSCALL_DECL_RETS10			\
	register uword_t rr4  __asm__("r9");		\
	register uword_t rr5 __asm__("r10");		\
	register uword_t rr6 __asm__("r12");		\
	register uword_t rr7 __asm__("r13");		\
	register uword_t rr8 __asm__("r14");		\
	register uword_t rr9 __asm__("r15")

#define REGS_SYSCALL_DECL_POSTRETS10			\
	*ret4 = rr4;					\
	*ret5 = rr5;					\
	*ret6 = rr6;					\
	*ret7 = rr7;					\
	*ret8 = rr8;					\
	*ret9 = rr9

#define REGS_SYSCALL_RET1			\
	"=a" (*ret0)

#define REGS_SYSCALL_RET4					\
	REGS_SYSCALL_RET1, "=b" (*ret1), "=d" (*ret2), "=S" (*ret3)

#define REGS_SYSCALL_RET10					\
	REGS_SYSCALL_RET4, "=r" (rr4), "=r" (rr5), "=r" (rr6), "=r" (rr7), "=r" (rr8), "=r" (rr9)

#define REGS_SYSCALL_ARG4						\
	"r" (r11_ctx), "a" (cap), "b" (ops), "d" (a0), "S" (a1), "D" (a2), "r" (r8)

#define REGS_SYSCALL_ARG10						\
	REGS_SYSCALL_ARG4, "r" (r9), "r" (r10), "r" (r12), "r" (r13), "r" (r14), "r" (r15)

#define REGS_SYSCALL_CLOBBER_BASE		\
	"memory", "cc"

#define REGS_SYSCALL_CLOBBER4						\
	REGS_SYSCALL_CLOBBER_BASE, "r9", "r10", "r12", "r13", "r14", "r15"

#define REGS_SYSCALL_CLOBBER10						\
	REGS_SYSCALL_CLOBBER_BASE

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
regs_syscall_4_1(cos_cap_t cap, cos_op_bitmap_t ops, uword_t a0, uword_t a1, uword_t a2, uword_t a3, uword_t *ret0) {
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS4;

	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET1
		     : REGS_SYSCALL_ARG4
		     : REGS_SYSCALL_CLOBBER4);
}

COS_FORCE_INLINE static inline void
regs_syscall_4_4(cos_cap_t cap, cos_op_bitmap_t ops,
		 uword_t a0, uword_t a1, uword_t a2, uword_t a3,
		 uword_t *ret0, uword_t *ret1, uword_t *ret2, uword_t *ret3) {
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS4;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET4
		     : REGS_SYSCALL_ARG4
		     : REGS_SYSCALL_CLOBBER4);
}

COS_FORCE_INLINE static inline void
regs_syscall_10_1(cos_cap_t cap, cos_op_bitmap_t ops,
		  uword_t a0, uword_t a1, uword_t a2, uword_t a3, uword_t a4, uword_t a5,
		  uword_t a6, uword_t a7, uword_t a8, uword_t a9, uword_t *ret0) {
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS10;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET1
		     : REGS_SYSCALL_ARG10
		     : REGS_SYSCALL_CLOBBER10);
}

COS_FORCE_INLINE static inline void
regs_syscall_10_4(cos_cap_t cap, cos_op_bitmap_t ops,
		  uword_t a0, uword_t a1, uword_t a2, uword_t a3, uword_t a4, uword_t a5,
		  uword_t a6, uword_t a7, uword_t a8, uword_t a9,
		  uword_t *ret0, uword_t *ret1, uword_t *ret2, uword_t *ret3) {
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS10;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET4
		     : REGS_SYSCALL_ARG10
		     : REGS_SYSCALL_CLOBBER10);
}

COS_FORCE_INLINE static inline void
regs_syscall_10_10(cos_cap_t cap, cos_op_bitmap_t ops,
		   uword_t a0, uword_t a1, uword_t a2, uword_t a3, uword_t a4, uword_t a5,
		   uword_t a6, uword_t a7, uword_t a8, uword_t a9,
		   uword_t *ret0, uword_t *ret1, uword_t *ret2, uword_t *ret3, uword_t *ret4,
		   uword_t *ret5, uword_t *ret6, uword_t *ret7, uword_t *ret8, uword_t *ret9) {
	REGS_SYSCALL_CTXT;
	REGS_SYSCALL_DECL_ARGS10;
	REGS_SYSCALL_DECL_RETS10;
	asm volatile(REGS_SYSCALL_TEMPLATE
		     : REGS_SYSCALL_RET10
		     : REGS_SYSCALL_ARG10
		     : REGS_SYSCALL_CLOBBER10);
	REGS_SYSCALL_DECL_POSTRETS10;
}

#endif	/* !__ASSEMBLER__ */
