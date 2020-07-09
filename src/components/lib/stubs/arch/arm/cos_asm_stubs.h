#ifndef COS_ASM_STUB_ARM_H
#define COS_ASM_STUB_ARM_H

/* clang-format off */
#ifdef COS_SERVER_STUBS
#include "../../../kernel/include/asm_ipc_defs.h"
//#include <consts.h>
#include <cos_asm_simple_stacks.h>

/*
 * This is the default, simple stub that is a slightly faster path.
 * Calls the server's function directly, instead of indirecting
 * through a C stub.  Passes 4 arguments in registers, and returns a
 * single value.
 *
 * The register layout is paired with that in ipc.S in the kernel,
 * %ecx holding the token.  We zero out the %ebp so that if we do a
 * stack trace later, we know that when the %ebp is 0, we are at the
 * end of the stack.
 */
#define cos_asm_stub(name)			\
.type  name, %function;				\
.weak name;					\
.globl __cosrt_s_##name;			\
.type  __cosrt_s_##name, %function ;		\
.align 16 ;					\
__cosrt_s_##name:				\
	COS_ASM_GET_STACK_INVTOKEN		\
	mov r0, r2;				\
	mov r1, r3;				\
	mov r2, r4;				\
	mov r3, r5;				\
	ldr r6, =name;				\
	bl r6 ;					\
	ldr r1, =RET_CAP;			\
	COS_ASM_RET_STACK			\
						\
	svc #0x00;				\
	.ltorg;

/*
 * This stub enables three return values (%ecx, %esi, %edi), AND
 * requires that you provide separate stub functions that are
 * indirectly activated and are written in C (in c_stub.c and s_stub.c
 * using cos_stubs.h) to choose the calling convention you'd like.
 */
#define cos_asm_stub_indirect(name)		\
.globl __cosrt_s_##name;			\
.type  __cosrt_s_##name, %function ;		\
.align 16 ;					\
__cosrt_s_##name:				\
        COS_ASM_GET_STACK_INVTOKEN              \
	COS_ASM_ALLOC_SP			\
	mov r6, sp;				\
	COS_ASM_ALLOC_SP			\
	mov r7, sp;				\
	mov r0, r2;				\
	mov r1, r3;				\
	mov r2, r4;				\
	mov r3, r5;				\
	mov r4, r6;				\
	mov r5, r7;				\
	ldr r8, =__cosrt_s_cstub_##name;	\
	bl r8 ;					\
	ldr r2, [r6];				\
	ldr r3, [r7];				\
						\
	ldr r1, =RET_CAP;			\
	COS_ASM_RET_STACK			\
	svc #0x00;				\
	.ltorg;

#endif

#ifdef COS_UCAP_STUBS
/*
 * This file contains the client-side assembly stubs for a synchronous
 * invocation (sinv).  It should be included in the assembly files
 * that constitute the stubs in the src/component/interface/.../stubs/
 * directories.  This part of the synchronous invocation path is very
 * much like the Procedure Linkage Table (PLT) and indirects through a
 * trampoline stub that parses a data-structure that locates the
 * actual stub.
 *
 * This code creates
 *
 * 1. the user-level capability (ucap) structure that contains the
 *    address of the stub to invoke when one of the server's interface
 *    functions is invoked, and
 *
 * 2. the trampoline that actually implements the function's symbol,
 *    and redirects the call to the stub, passing a reference to the
 *    ucap to it.  That ucap contains the capability that should be
 *    used for the kernel sinv.
 */

//#define __ASM__
#include "../../../kernel/include/asm_ipc_defs.h"
//#include <consts.h>

/*
 * Note that all ucaps are allocated into a separate section so that
 * they are contiguous.  That section is then collapsed with .data
 * during the build process (via comp.ld).
 *
 * The __cosrt_extern_* aliases are to support components to call a
 * server function in an interface that it also exports.
 */

#define cos_asm_stub(name)		       \
.text;                                         \
.weak name;                                    \
.globl __cosrt_extern_##name;                  \
.type  name, %function;			       \
.type  __cosrt_extern_##name, %function;       \
.align 8 ;                                     \
name:                                          \
__cosrt_extern_##name:			       \
	ldr r6, =__cosrt_ucap_##name;	       \
	ldr r6, [r6, #INVFN];		       \
	bl   r6;			       \
	.ltorg;				       \
					       \
.section .ucap, "a", %progbits ;               \
.globl __cosrt_ucap_##name ;                   \
__cosrt_ucap_##name:                           \
        .rep UCAP_SZ ;                         \
        .long 0 ;                              \
        .endr ;				       \
.text /* start out in the text segment, and always return there */

#define cos_asm_stub_indirect(name) cos_asm_stub(name)
#endif

.text

/* clang-format on */

#endif	/* COS_ASM_STUB_ARM_H */
