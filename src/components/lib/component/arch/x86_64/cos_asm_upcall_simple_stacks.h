/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_STATIC_STACK				\
/* .bss declaration must be put at the beginning */	\
.bss;							\
.align COS_STACK_SZ;					\
.globl cos_static_stack;				\
cos_static_stack:					\
	.rep ALL_STACK_SZ_FLAT;				\
	.8byte 0	;				\
	.endr ;						\
.globl cos_static_stack_end;				\
cos_static_stack_end:

#define COS_UPCALL_ENTRY 		\
.text;					\
.globl __cosrt_upcall_entry;		\
.type __cosrt_upcall_entry, @function;	\
.align 16;				\
__cosrt_upcall_entry:			\
	COS_ASM_GET_STACK		\
	push $0;			\
	mov %rsp, -16(%rsp);		\
	push $0;			\
	mov %rsp, -16(%rsp);		\
	sub $16, %rsp;			\
	mov %rsi, %rcx;			\
	mov %rdi, %rdx;			\
	mov %rbx, %rsi;			\
	xor %rbp, %rbp;			\
	mov %r12, %rdi;			\
	/* ABI mandate a 16-byte alignment stack pointer*/ \
	and $~0xf, %rsp;		\
	call cos_upcall_fn;		\
	addl $24, %esp;			\
	pop %rsi;			\
	pop %rdi;			\
	movl %eax, %ecx;		\
	movl $RET_CAP, %eax;		\
	COS_ASM_RET_STACK		\
	sysenter;

#define COS_ATOMIC_CMPXCHG 		\
	movl %eax, %edx;		\
	cmpl (%ebx), %eax;		\
	jne cos_atomic_cmpxchg_end;	\
	movl %ecx, %edx;		\
	movl %ecx, (%ebx);

#define COS_ATOMIC_CMPXCHG_END		\
	ret;

#define COS_ATOMIC_USER4_END		\
	movl $0, %eax;			\
	movl (%eax), %eax;
