#pragma once

#include <cos_types.h>
#include <cos_arch_consts.h>

typedef uword_t        vaddr_t;           /* opaque, user-level virtual address */
typedef u64_t          prot_domain_tag_t; /* additional architecture protection domain context (e.g. PKRU, ASID) */
typedef uword_t        reg_state_t;       /* the state of a register set: preempted, or partially saved  */

#define REG_STATE_PREEMPTED 0 /* The registers must be fully restored as they represent preempted state */
#define REG_STATE_SYSCALL   1 /* The registers don't require full restoration, and can use `sysret` */

#define REGS_NUM_ARGS_RETS  13
#define REGS_RETVAL_BASE    0

#define REGS_ARG_CAP        0
#define REGS_ARG_OPS        1

/*
 * The trap frame added onto the stack by interrupts/exceptions. This
 * is only valid/used for traps (interrupts or exceptions), where
 * `regs.state == REG_STATE_PREEMPTED`. Otherwise we have a constant
 * flags value for user-level, and should use the `ip`/`sp` in `struct
 * sysret_clobbered`.
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
 * on returning to user-level with sysret. We use these to pass ip/sp
 * from user-level, and to restore ip/flags on return to user-level.
 * When a trap occurs, these are simply normal `rcx`/`r11` registers
 * that must be fully restored.
*/
struct sysret_clobbered {
	/* Used to hold instruction pointer on syscall/sysret */
	uword_t rcx_ip;
        /*
	 * Used to store stack pointer on syscall. r11 holds rflags on
         * syscall return. This is the top address of the `struct
         * regs` structure to be restored on system call so that the
         * stack pointer can be restored after restoring the rest, and
         * directly before returning to user-level.
	 */
	uword_t r11_sp;
};

/*
 * The register set for a thread. This structure exists one
 * per-thread, and once on each core's stack.
 *
 * The `args` are generally used to pass data as function arguments or
 * return values. They are ordered as such:
 *
 * rax, rbx, rdx, rsi, rdi, rbp, r8, r9, r10, r12, r13, r14, r15
 *
 * Note the rcx and r11 are clobbered as part of `sysret`, thus follow
 * the other GP regs in `struct sysret_clobbered`.
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
	struct sysret_clobbered clobbered;
	reg_state_t state;	/* `0` = trap, `1` = syscall */
	struct trap_frame frame;
};

/**
 * `regs_arg` retrieves a thread's argument. This should only be used
 * when the register's state (`rs->state`) is `REG_STATE_SYSCALL`.
 *
 * Conventions for these arguments when we're making a typical system
 * call include:
 *
 * - `0`: the capability activated with the system call
 * - `1` - `12`: Activation arguments
 *
 * Conventions for returning from a synchronous invocation or
 * calling `reply_and_wait` include:
 *
 * - `0` - normal function return value
 * - `1` - `12` - additional return values
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
 * - `1` - `12`: additional return values, where appropriate
 *
 * Conventions for these return values for an *upcall* corresponding
 * to a synchronous invocation into a server component include:
 *
 * - `0`: core id
 * - `1`: thread id
 * - `2`: the synchronous invocation token
 * - `3` - `12`: the function arguments, from 3 to 12th.
 *
 * Conventions for these return values for a `call`-based activation
 * of a thread through IPC include:
 *
 * - `0`: core id
 * - `1`: thread id
 * - `2`: the end-point token
 * - `3`: the client identifier token
 * - `4` - `12`: the function arguments, from 4 to 12th.
 */
COS_FORCE_INLINE static inline uword_t
regs_retval(struct regs *rs, int retvalno, uword_t val)
{
	return rs->args[retvalno] = val;
}

/**
 * `regs_ip_sp` simply returns the thread's instruction and stack
 * pointers. Assumes that this should only be used if `rs->state ==
 * REG_STATE_SYSCALL`.
 */
COS_FORCE_INLINE static inline void
regs_ip_sp(struct regs *rs, uword_t *ip, uword_t *sp)
{
	*ip = rs->clobbered.rcx_ip;
	*sp = rs->clobbered.r11_sp;
}

/**
 * `regs_set_ip_sp` sets the register set's instruction and stack
 * pointers. Assumes that this should only be used if `rs->state ==
 * REG_STATE_SYSCALL`.
 */
COS_FORCE_INLINE static inline void
regs_set_ip_sp(struct regs *rs, uword_t ip, uword_t sp)
{
	rs->clobbered.rcx_ip = ip;
	rs->clobbered.r11_sp = sp;
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
	rs->clobbered.rcx_ip = entry_ip;
	rs->clobbered.r11_sp = 0;

	return;
}
