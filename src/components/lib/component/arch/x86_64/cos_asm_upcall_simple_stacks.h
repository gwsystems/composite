/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_STATIC_STACK				\
/* .bss declaration must be put at the beginning */	\
.data;							\
.align COS_STACK_SZ;					\
.weak cos_static_stack;					\
.globl cos_static_stack;				\
cos_static_stack:					\
	.rep ALL_STACK_SZ_FLAT;				\
	.byte 0	;					\
	.endr ;						\
.weak cos_static_stack_end;				\
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


/* This is a very critical path used by both the upcall and synchronous IPC, thus make sure you fully understand it and change it */
/* Be very very careful of the registers used here, you only would want to use ax and dx and don't change other registers as they could possibly be used by upcall and IPC */
#define COS_DEFAULT_STACK_ACQUIRE						\
.text;										\
.align 16;									\
.weak custom_acquire_stack;							\
.globl custom_acquire_stack;							\
custom_acquire_stack:								\
	/* ax holds cpuid and thread id*/					\
	/* rax[0:15]=tid, rax[16:31]=cpuid */					\
	movq %rax, %rdx;							\
	movabs $cos_static_stack, %rsp;						\
	/*rax hols coreid and thread id, do not use other registers! */		\
	/* get the tid by masking rax[0:15] */					\
	andq $0xffff, %rax;							\
	/* simple math: this thread's stack offset = stack_size * tid */	\
	shl $MAX_STACK_SZ_BYTE_ORDER, %rax;					\
	/* add the stack offset to the base to get the current thread's stack*/	\
	add %rax, %rsp;								\
	/* restore the tid in order to save it on the stack*/			\
	shr $MAX_STACK_SZ_BYTE_ORDER, %rax;					\
	/* get the cpuid by right shifting the lower 16 bits*/			\
	shr $16, %rdx;								\
	/* on the return, rax is thread id, rdx is core id */ 			\
	jmpq %rcx;
