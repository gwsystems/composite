#ifndef COS_ASM_SIMPLE_STACKS_X86_64_H
#define COS_ASM_SIMPLE_STACKS_X86_64_H

/* clang-format off */
#define COS_ASM_GET_STACK_BASIC									\
	/* we save the return position to rcx for custom stack acquisition code to return */ 	\
	/* Make sure %r13 is not used for other purpose. */                                     \
	movabs $1f, %r13;									\
	/* jump to stack acquisition code */ 							\
	jmp custom_acquire_stack;								\
1:												\
	/* save the cpuid on the top of the thread stack */					\
	pushq %rdx;										\
	/* save the tid followed by the cpuid */						\
	pushq %rax;

#define COS_ASM_GET_STACK       \
	COS_ASM_GET_STACK_BASIC \
	push $0;

#define COS_ASM_GET_STACK_INVTOKEN \
	COS_ASM_GET_STACK_BASIC    \
	push %rbp;

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

#define COS_SIMPLE_STACK_THDID_OFF 0x1fff0

/*
 * pkru = ~(0b11 << (2 * pkey)) & ~0b11;
 * this disables W/R for all pkeys except
 * the pkey we want to enable. 
 * See intels manual for pkru details
 */
#define COS_ULINV_SWITCH_DOMAIN(protdom)			\
	movl	$protdom,  %ecx;				\
	addl	%ecx,      %ecx;				\
	movl	$0b11,     %eax;				\
	sall	%cl,       %eax;				\
	notl	%eax;						\
	andl	$-4,       %eax;				\
	xor	%rcx,      %rcx;				\
	xor	%rdx,      %rdx;				\
	wrpkru;	

/* 
 * Get a pointer to this thread's user-level
 * invocation stack. see typedef ulk_invstk
 * - input: r13 = cpuid << 16 | tid
 * - output: r14 = pointer to stack
 */
#define COS_ULINV_GET_INVSTK					\
	movabs	$ULK_INVSTK_ADDR, %r14;				\
	/* get perthread invstack */				\
	movq	%r13, %rax;					\
	andq	$0xffff, %rax;					\
	shlq    $8, %rax;	    				\
	/* r14 = &stack */	 				\
	addq    %rax, %r14; 					


/* 
 * push an entry onto this thread's user-level
 * invocation stack. 
 * - input: r14 = pointer to stack
 */
#define COS_ULINV_PUSH_INVSTK					\
	movq    (%r14), %rdx;					\
	addq    $1,     %rdx;					\
	shlq    $4,     %rdx;					\
	/* rdx = &stack_entry */	 			\
	addq    %r14,   %rdx;					\
	/* store cap no and sp */				\
	movq    $0x0123456789abcdef, %rax; 			\
	movq    %rax, (%rdx); 					\
	movq    %rsp, 8(%rdx); 					\
	/* increment top-of-stack */				\
	movq    (%r14), %rax;					\
	addq    $1, %rax;					\
	movq    %rax, (%r14);		

/* 
 * push an entry onto this thread's user-level
 * invocation stack. 
 * - input: r14 = pointer to stack
 */
#define COS_ULINV_POP_INVSTK					\
	movq    (%r14), %rdx;					\
	shlq    $4,     %rdx;					\
	/* rdx = &stack_entry */	 			\
	addq    %r14,   %rdx;					\
	/* restore sp */					\
	movq    8(%rdx),%rsp; 					\
	/* decrement top-of-stask index */			\
	movq    (%r14), %rax;					\
	subq    $1, %rax;					\
	movq    %rax, (%r14);					


/* clang-format on */

#endif
