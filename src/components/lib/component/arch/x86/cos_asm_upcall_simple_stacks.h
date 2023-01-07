/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_STATIC_STACK		\
.align COS_STACK_SZ;			\
.bss;					\
.globl cos_static_stack;		\
cos_static_stack:			\
	.rep ALL_STACK_SZ_FLAT;		\
	.4byte 0;			\
	.endr;				\
.globl cos_static_stack_end;		\
cos_static_stack_end:

#define COS_UPCALL_ENTRY 		\
.text;					\
.globl __cosrt_upcall_entry;		\
.type __cosrt_upcall_entry, @function;	\
.align 16;				\
__cosrt_upcall_entry:			\
	COS_ASM_GET_STACK		\
	pushl $0;			\
	movl %esp, -8(%esp);		\
	pushl $0;			\
	movl %esp, -8(%esp);		\
	subl $8, %esp;			\
	pushl %esi;			\
	pushl %edi;			\
	pushl %ebx;			\
	xor %ebp, %ebp;			\
	pushl %ecx;			\
	call cos_upcall_fn;		\
	addl $24, %esp;			\
	popl %esi;			\
	popl %edi;			\
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
