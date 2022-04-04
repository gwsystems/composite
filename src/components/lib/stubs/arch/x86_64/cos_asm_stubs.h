#ifndef COS_ASM_STUB_X86_64_H
#define COS_ASM_STUB_X86_64_H

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
#define cos_asm_stub(name)					\
.globl __cosrt_s_##name;					\
.type  __cosrt_s_##name, @function ;				\
.align 16 ;							\
__cosrt_s_##name:						\
	COS_ASM_GET_STACK_INVTOKEN				\
	mov %r12, %rcx;						\
	xor %rbp, %rbp;						\
	mov %rdi, %rax;						\
	mov %rbx, %rdi;						\
	mov %rax, %rdx;						\
	/* ABI mandate a 16-byte alignment stack pointer*/	\
	and $~0xf, %rsp;					\
	call name ;						\
 	/* addl $16, %esp; */					\
	mov %rax, %r8;						\
	mov $RET_CAP, %rax;					\
	COS_ASM_RET_STACK					\
	syscall;

#define cos_asm_stub_shared(name)				\
.globl __cosrt_s_##name;					\
.type  __cosrt_s_##name, @function ;				\
.align 16 ;							\
__cosrt_s_##name:						\
	/* TODO: save stack on inv stack */			\
	movq    %rsp, %r14;					\
	movq    $0xdeadbeefdeadbeef, %r15; 			\
								\
	/* switch to server execution stack */			\
	COS_ASM_GET_STACK_INVTOKEN				\
								\
	/* TODO: ULK */						\
								\
	/* switch to server protection domain */		\
	movl    $0xfffffffe, %eax;				\
	xor     %rcx, %rcx;					\
	xor     %rdx, %rdx;					\
	wrpkru;  						\
								\
	/* check client token */				\
	movq    $0xdeadbeefdeadbeef, %rax;			\
	cmp     %rax, %r15;					\
	/* TODO: jne     bad */					\
								\
	/* ABI mandate a 16-byte alignment stack pointer*/	\
	and 	$~0xf, %rsp;					\
	xor 	%rbp, %rbp;					\
	callq   name;						\
	movq	%rax, %r8;					\
								\
	/* save server authentication token */			\
	movq    $0xdeadbeefdeadbeef, %r15;			\
								\
	/* TODO: ULK */						\
								\
	/* switch back to client protection domain */		\
	movl    $0xfffffffe, %eax;					\
	xor     %rcx, %rcx;					\
	xor     %rdx, %rdx;					\
	wrpkru;							\
								\
	/* TODO: restore stack from inv stack */		\
	movq	%r14, %rsp;					\
								\
	/* check server token */				\
	movq    $0xdeadbeefdeadbeef, %rax;			\
	cmp     %rax, %r15;					\
	/* TODO: jne     bad */					\
								\
	movq    %r8, %rax;						\
	retq;							\

/*
 * This stub enables three return values (%ecx, %esi, %edi), AND
 * requires that you provide separate stub functions that are
 * indirectly activated and are written in C (in c_stub.c and s_stub.c
 * using cos_stubs.h) to choose the calling convention you'd like.
 */
#define cos_asm_stub_indirect(name)		\
.globl __cosrt_s_##name;			\
.type  __cosrt_s_##name, @function ;		\
.align 16 ;					\
__cosrt_s_##name:				\
	COS_ASM_GET_STACK_INVTOKEN		\
	/* 16-byte alignment of rsp*/		\
	and $~0xf, %rsp;			\
	push $0;				\
	mov %rsp, %r8;				\
	push $0;				\
	mov %rsp, %r9;				\
	mov %r12, %rcx;				\
	xor %rbp, %rbp;				\
	mov %rdi, %r12;				\
	mov %rbx, %rdi;				\
	mov %r12, %rdx;				\
	call __cosrt_s_cstub_##name ;		\
	pop %rdi;				\
	pop %rsi;				\
	mov %rax, %r8;				\
	mov $RET_CAP, %rax;			\
	COS_ASM_RET_STACK			\
	syscall;
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
#include <consts.h>

/*
 * Note that all ucaps are allocated into a separate section so that
 * they are contiguous.  That section is then collapsed with .data
 * during the build process (via comp.ld).
 *
 * The __cosrt_extern_* aliases are to support components to call a
 * server function in an interface that it also exports.
 */

#define cos_asm_stub(name)			\
.text;						\
.weak name;					\
.globl __cosrt_extern_##name;			\
.type  name, @function;				\
.type  __cosrt_extern_##name, @function;	\
.align 8 ;					\
name:						\
__cosrt_extern_##name:				\
	movabs $__cosrt_ucap_##name, %rax ;	\
	jmp *INVFN(%rax) ;			\
						\
.section .ucap, "a", @progbits ;		\
.globl __cosrt_ucap_##name ;			\
__cosrt_ucap_##name:				\
	.rep UCAP_SZ ;				\
	.quad 0 ;				\
	.endr ;					\
.text /* start out in the text segment, and always return there */

#define cos_asm_stub_indirect(name) cos_asm_stub(name)

#define cos_asm_stub_shared(name)		\
.text;						\
.weak name;					\
.globl __cosrt_extern_##name;			\
.type  name, @function;				\
.type  __cosrt_extern_##name, @function;	\
.align 8 ;					\
name:						\
__cosrt_extern_##name:				\
	movabs $__cosrt_ucap_##name, %rax ;	\
	callq *INVFN(%rax) ;			\
	retq ;					\
						\
.section .ucap, "a", @progbits ;		\
.globl __cosrt_ucap_##name ;			\
__cosrt_ucap_##name:				\
	.rep UCAP_SZ ;				\
	.quad 0 ;				\
	.endr ;					\
.text /* start out in the text segment, and always return there */

#endif

.text

/* clang-format on */

#endif	/* COS_ASM_STUB_X86_H */
