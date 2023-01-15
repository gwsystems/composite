#ifndef COS_ASM_STUB_X86_64_H
#define COS_ASM_STUB_X86_64_H

#include <cos_asm_simple_stacks.h>

/* clang-format off */
#ifdef COS_SERVER_STUBS
#include "../../../kernel/include/asm_ipc_defs.h"
//#include <consts.h>

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
	syscall;						\
								\
.global __cosrt_alts_##name ;					\
.type  __cosrt_alts_##name, @function ;				\
.align 16 ;							\
__cosrt_alts_##name: 						\
	movq	%r13, %rax;					\
	/* switch to server execution stack */			\
	COS_ASM_GET_STACK_INVTOKEN				\
	/* ABI mandate a 16-byte alignment stack pointer*/	\
	and 	$~0xf, %rsp;					\
	xor 	%rbp, %rbp;					\
	pushq	%rcx;						\
	/* stack alignment */					\
	pushq	%rcx;						\
	movq    %r8, %rcx;					\
	movq    %r9, %rdx;					\
	callq   name;						\
	popq	%rcx;						\
	retq ;							\
								
/*
 * This stub enables three return values (%ecx, %esi, %edi), AND
 * requires that you provide separate stub functions that are
 * indirectly activated and are written in C (in c_stub.c and s_stub.c
 * using cos_stubs.h) to choose the calling convention you'd like.
 */
#define cos_asm_stub_indirect(name)				\
.globl __cosrt_s_##name;					\
.type  __cosrt_s_##name, @function ;				\
.align 16 ;							\
__cosrt_s_##name:						\
	COS_ASM_GET_STACK_INVTOKEN				\
	/* 16-byte alignment of rsp*/				\
	and $~0xf, %rsp;					\
	push $0;						\
	mov %rsp, %r8;						\
	push $0;						\
	mov %rsp, %r9;						\
	mov %r12, %rcx;						\
	xor %rbp, %rbp;						\
	mov %rdi, %r12;						\
	mov %rbx, %rdi;						\
	mov %r12, %rdx;						\
	call __cosrt_s_cstub_##name ;				\
	pop %rdi;						\
	pop %rsi;						\
	mov %rax, %r8;						\
	mov $RET_CAP, %rax;					\
	COS_ASM_RET_STACK					\
	syscall;						\
.global __cosrt_alts_##name ;					\
.type  __cosrt_alts_##name, @function ;				\
.align 16 ;							\
__cosrt_alts_##name: 						\
	movq	%r13, %rax;					\
	/* switch to server execution stack */			\
	COS_ASM_GET_STACK_INVTOKEN				\
	/* ABI mandate a 16-byte alignment stack pointer*/	\
	and 	$~0xf, %rsp;					\
	xor 	%rbp, %rbp;					\
	pushq	%rcx;						\
	/* stack alignment */					\
	pushq	%rcx;						\
	movq    %r8, %rcx;					\
	movq    %r9, %rdx;					\
	pushq	$0;						\
	movq	%rsp, %r8;					\
	pushq	$0;						\
	movq	%rsp, %r9;					\
	callq   __cosrt_s_cstub_##name;				\
	popq	%rdi;						\
	popq	%rsi;						\
	popq	%rcx;						\
	retq ;							
						

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
 * 
 * __cosrt_fast_callgate_* is the trampoline for making user-level
 * synchronous invocations. It does the following:
 * 	- save callee saved registers to the client stack
 * 	- save the parameters in rcx and rdx; we will need these 
 * 		for scratch registers
 * 	- load the client AUTH token into r15; this value is JITed
 * 		at boot time
 * 	- get the tid off the stack
 * 	- use the tid to index into the user-level kernel memory
 * 		and get a pointer to this thread's user-level 
 * 		invocation stack
 * 	- switch to the user-level kernel's protection domain
 * 	- push the call's invocation stack from
 * 		- the client stack
 * 		- the sinvcap for kernel usage
 * 	- switch to the server's protection domain
 * 	- put the invocation token into rbp, just like in 
 * 		a normal sinv
 * 	- check the client AUTH token to verify we got without
 * 		tampering
 * 	- save the return address into rcx; we are switching stacks
 * 		and cant using the stack for control flow
 * 	- jump to the server side of the callgate. this:
 * 		- gets the server's stack 
 * 		- sets up the server's stack with the
 * 			invocation token and thdid
 * 		- push the return address we put in rcx
 * 		- call the actual function and return to the client
 * 	- save the return value, we will need rax
 * 	- load the server AUTH token into r15
 * 	- get the thread id and use it to the get UL inv stack again
 * 	- pop the UL-inv stack entry we pushed
 * 	- switch back to the client's protection domain
 * 	- check the server's AUTH token
 * 	- put the return value back into rax
 * 	- pop callee saved regs
 *
 */

#define UL_KERNEL_MPK_KEY 0x01

#define cos_asm_stub(name)					\
.text;								\
.weak name;							\
.globl __cosrt_extern_##name;					\
.type  name, @function;						\
.type  __cosrt_extern_##name, @function;			\
.align 16 ;							\
name:								\
__cosrt_extern_##name:						\
	movabs $__cosrt_ucap_##name, %rax ;			\
	callq *INVFN(%rax) ;					\
	retq ;							\
.global  __cosrt_fast_callgate_##name;				\
.type  __cosrt_fast_callgate_##name, @function;			\
.align 16 ;							\
__cosrt_fast_callgate_##name:					\
	/* callee saved */					\
	pushq	%rbp;						\
	pushq	%r13; /* tid | cpuid << 16 */			\
	pushq	%r14; /* struct ulk_invstk ptr  */		\
	pushq	%r15; /* auth tok */				\
	movq    %rcx, %r8;					\
	movq    %rdx, %r9;					\
	movq    $0xdeadbeefdeadbeef, %r15; 			\
	/* thread ID and cpu ID */				\
	movq    %rsp, %rdx;					\
	andq    $0xfffffffffffe0000, %rdx;			\
	movzwq  COS_SIMPLE_STACK_THDID_OFF(%rdx), %r13;		\
	movzwq  COS_SIMPLE_STACK_CPUID_OFF(%rdx), %rax;		\
	shl	$16, %rax;					\
	or	%rax, %r13;					\
	COS_ULINV_GET_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(UL_KERNEL_MPK_KEY)		\
	COS_ULINV_PUSH_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(0xfffffffe)			\
	/* invocation token */					\
	movabs  $0x0123456789abcdef, %rbp;			\
	/* check client token */				\
	movq    $0xdeadbeefdeadbeef, %rax;			\
	cmp     %rax, %r15;					\
	/* TODO: jne     bad */					\
	movabs	$0x1212121212121212, %rax;			\
	movabs	$srv_call_ret_##name, %rcx;			\
	jmpq   *%rax;						\
srv_call_ret_##name:						\
	movq	%rax, %r8;					\
	/* save server authentication token */			\
	movq    $0xdeadbeefdeadbeef, %r15;			\
	/* thread ID */						\
	movq    %rsp, %rdx;					\
	andq    $0xfffffffffffe0000, %rdx;			\
	movzwq  COS_SIMPLE_STACK_THDID_OFF(%rdx), %r13;		\
	COS_ULINV_GET_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(0x01)				\
	COS_ULINV_POP_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(0xfffffffe)			\
	/* check server token */				\
	movq    $0xdeadbeefdeadbeef, %rax;			\
	cmp     %rax, %r15;					\
	/* TODO: jne     bad */					\
	movq    %r8, %rax;					\
	/* callee saved */					\
	popq	%r15;						\
	popq	%r14;						\
	popq	%r13;						\
	popq	%rbp;						\
	retq;							\
								\
								\
.section .ucap, "a", @progbits ;				\
.globl __cosrt_ucap_##name ;					\
__cosrt_ucap_##name:						\
	.rep UCAP_SZ ;						\
	.quad 0 ;						\
	.endr ;							\
.text /* start out in the text segment, and always return there */


#define cos_asm_stub_indirect(name)				\
.text;								\
.weak name;							\
.globl __cosrt_extern_##name;					\
.type  name, @function;						\
.type  __cosrt_extern_##name, @function;			\
.align 16 ;							\
name:								\
__cosrt_extern_##name:						\
	movabs $__cosrt_ucap_##name, %rax ;			\
	callq *INVFN(%rax) ;					\
	retq ;							\
.global  __cosrt_fast_callgate_##name;				\
.type  __cosrt_fast_callgate_##name, @function;			\
.align 16 ;							\
__cosrt_fast_callgate_##name:					\
	/* callee saved */					\
	pushq	%rbp;						\
	pushq	%rbx;						\
	pushq	%r12;						\
	pushq	%r13; /* tid | cpuid << 16 */			\
	pushq	%r14; /* struct ulk_invstk ptr  */		\
	pushq	%r15; /* auth tok */				\
	/* save the two return ptrs in perserved regs */	\
 	movq	%r8, %r12;					\
 	movq	%r9, %rbx;					\
	movq    %rcx, %r8;					\
	movq    %rdx, %r9;					\
	movq    $0xdeadbeefdeadbeef, %r15; 			\
	/* thread ID and cpu ID */				\
	movq    %rsp, %rdx;					\
	andq    $0xfffffffffffe0000, %rdx;			\
	movzwq  COS_SIMPLE_STACK_THDID_OFF(%rdx), %r13;		\
	movzwq  COS_SIMPLE_STACK_CPUID_OFF(%rdx), %rax;		\
	shl	$16, %rax;					\
	or	%rax, %r13;					\
	COS_ULINV_GET_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(UL_KERNEL_MPK_KEY)		\
	COS_ULINV_PUSH_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(0xfffffffe)			\
	/* invocation token */					\
	movabs  $0x0123456789abcdef, %rbp;			\
	/* check client token */				\
	movq    $0xdeadbeefdeadbeef, %rax;			\
	cmp     %rax, %r15;					\
	/* TODO: jne     bad */					\
	movabs	$0x1212121212121212, %rax;			\
	movabs	$srv_call_ret_##name, %rcx;			\
	jmpq   	*%rax;						\
srv_call_ret_##name:						\
	movq	%rax, %r8;					\
	/* save server authentication token */			\
	movq    $0xdeadbeefdeadbeef, %r15;			\
	/* thread ID */						\
	movq    %rsp, %rdx;					\
	andq    $0xfffffffffffe0000, %rdx;			\
	movzwq  COS_SIMPLE_STACK_THDID_OFF(%rdx), %r13;		\
	COS_ULINV_GET_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(0x01)				\
	COS_ULINV_POP_INVSTK					\
	COS_ULINV_SWITCH_DOMAIN(0xfffffffe)			\
	/* check server token */				\
	movq    $0xdeadbeefdeadbeef, %rax;			\
	cmp     %rax, %r15;					\
	/* TODO: jne     bad */					\
	movq    %r8, %rax;					\
	movq	%rsi, (%r12);					\
	movq	%rdi, (%rbx);					\
	/* callee saved */					\
	popq	%r15;						\
	popq	%r14;						\
	popq	%r13;						\
	popq	%r12;						\
	popq	%rbx;						\
	popq	%rbp;						\
	retq;							\
								\
								\
.section .ucap, "a", @progbits ;				\
.globl __cosrt_ucap_##name ;					\
__cosrt_ucap_##name:						\
	.rep UCAP_SZ ;						\
	.quad 0 ;						\
	.endr ;							\
.text /* start out in the text segment, and always return there */			\

#endif

.text

/* clang-format on */

#endif	/* COS_ASM_STUB_X86_H */
