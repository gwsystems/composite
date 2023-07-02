#pragma once

#define STATE_KERNEL_STACK_SZ 3848
#define STATE_REGS_SZ         176
#define STATE_GLOBALS_SZ      56
#define STATE_REGS_OFFSET     3856
#define STATE_GLOBALS_OFFSET  4040
#define STATE_STACK_OFFSET    4032

#ifndef __ASSEMBLER__

#include <types.h>
#include <chal_regs.h>
#include <consts.h>
#include <state.h>
#include <compiler.h>

/**
 * The x86_64 state_percore is the per-core data-structure maintaining
 * the global roots of the core's state. This is rather awkward and
 * very specific, and coordinates with the assembly constants above.
 * The constraints/optimizations:
 *
 * - We want our global variables to share the same cache-lines as the
 *   kernel stack, and the gs pointer to the kernel stack (see below),
 *   so that we benefit from the effective prefetching.
 *
 * - We want to be able to retrieve the global state, and the
 *   registers using only the current stack pointer.
 *
 * - We have to have a pointer that the gs segment points to which we
 *   use to find the kernel stack (`gs_stack_ptr`).
 *
 * - The stack grows from higher addresses, to lower on x86_64.
 *
 * - The stack pointer (in any C function) assumes that we're pointing
 *   to an offset of 16 + 8 (8 is the return address pushed from
 *   `call`). Thus we must align the kernel stack at which we start
 *   executing C properly.
 *
 * - We want the `registers` to be part of the kernel stack, and when
 *   we receive a syscall/interrupt/exception, we always populate the
 *   structure by saving the registers.
 *
 * Given these constraints, we start the structure with the kernel
 * stack to populate the lower addresses. At the top of the kernel
 * stack is the registers (thus they are next in the structure). The
 * end of the registers structure is where the kernel stack is set to
 * start at kernel entry. Then we have the pointer to the stack to be
 * referenced by the `gs` segment on kernel entry, and all of our
 * core-local global variables. We place the `gs_stack_ptr` in between
 * registers and globals so that accessing that pointer has the
 * welcome side-effect of prefetching both the stack and the globals.
 *
 * The constants above are used by assembly, thus we statically assert
 * that they are both correct, and that the structure satisfies the
 * above constraints.
 */
struct state_percore {
	u64_t redzone; 		/* detect overflows */
	u8_t kernel_stack[STATE_KERNEL_STACK_SZ];
	struct regs registers;
	char *gs_stack_ptr;
	struct state globals;
} COS_PAGE_ALIGNED;

#define STATE_OFFSETOF_REGS    offsetof(struct state_percore, registers)
#define STATE_OFFSETOF_GLOBALS offsetof(struct state_percore, globals)

COS_STATIC_ASSERT(STATE_REGS_SZ == sizeof(struct regs),
		  "The constant for the size of the registers is not correct");
COS_STATIC_ASSERT(STATE_GLOBALS_SZ == sizeof(struct state),
		  "The constant for the size of the global state is not correct");
COS_STATIC_ASSERT((STATE_OFFSETOF_REGS / 16) * 16 == STATE_OFFSETOF_REGS,
                  "Registers are not 16 byte aligned in per-core state.");
COS_STATIC_ASSERT(STATE_REGS_OFFSET + STATE_REGS_SZ + 8 == STATE_OFFSETOF_GLOBALS,
		  "The stack offset constant into per-core state is not correct.");
COS_STATIC_ASSERT(COS_PAGE_SIZE == sizeof(struct state_percore),
		  "The size of the per-core state is not correct");
/* The stack is to start at the end of `registers` */
COS_STATIC_ASSERT((STATE_STACK_OFFSET / 16 ) * 16 == STATE_STACK_OFFSET,
		  "The kernel stack's constant is not an architecture-mandated value aligned on a 16-byte boundary.");
COS_STATIC_ASSERT(STATE_OFFSETOF_GLOBALS == STATE_GLOBALS_OFFSET,
		  "The globals offset into per-core data is incorrect.");
COS_STATIC_ASSERT(STATE_OFFSETOF_REGS == STATE_REGS_OFFSET,
		  "The register offset into per-core data is incorrect.");

extern struct state_percore core_state[COS_NUM_CPU];

/*
 * Here, we're using the stack pointer to find the per-core state. We
 * could use `gs` along with a pointer to the structure instead.
 * However, assuming that L1 is 3-4 cycles, we should be able to do a
 * mask faster than accessing the pointer using that technique.
 */
COS_FORCE_INLINE static inline struct state_percore *
chal_percore_state(void)
{
	char v;	/* variable allocated on the stack whose address we'll use */

	return (struct state_percore *)(((u64_t)&v) & (~(COS_PAGE_SIZE - 1)));
}

COS_FORCE_INLINE static inline struct state *
state(void)
{
	return &chal_percore_state()->globals;
}

COS_FORCE_INLINE static inline struct regs *
current_registers(void)
{
	return &chal_percore_state()->registers;
}

void chal_state_init(void);

#endif	/* !__ASSEMBLER__ */
