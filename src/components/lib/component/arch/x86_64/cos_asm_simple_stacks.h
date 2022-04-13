#ifndef COS_ASM_SIMPLE_STACKS_X86_64_H
#define COS_ASM_SIMPLE_STACKS_X86_64_H

/* clang-format off */
#define COS_ASM_GET_STACK_BASIC             \
	movabs $cos_static_stack, %rsp;	    \
	mov %rax, %rdx;		    \
	andq $0xffff, %rax;		    \
	shl $MAX_STACK_SZ_BYTE_ORDER, %rax; \
	add %rax, %rsp;		    \
	shr $MAX_STACK_SZ_BYTE_ORDER, %rax; \
	shr $16, %rdx;			    \
	pushq %rdx;			    \
	pushq %rax;

#define COS_ASM_GET_STACK       \
	COS_ASM_GET_STACK_BASIC \
	push $0;

#define COS_ASM_GET_STACK_INVTOKEN \
	COS_ASM_GET_STACK_BASIC    \
	push %rbp;

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

#define COS_ULINV_SWITCH_DOMAIN(pkru)				\
	movl    $pkru, %eax;					\
	xor     %rcx, %rcx;					\
	xor     %rdx, %rdx;					\
	wrpkru;	

/* r13 = tid, r14 = isb ptr */
#define COS_ULINV_PUSH_INVSTK					\
	movq    %r14,   %rdx;					\
	movq    %r13,   %rax;					\
	/* get perthread invstack */				\
	shlq    $7,     %rax;    				\
	addq    %rdx,   %rax; 					\
	/* rcx = &stack->top */	 				\
	movq    %rax,   %rcx;					\
	movq    (%rcx), %rdx;					\
	addq    $1,     %rdx;					\
	shlq    $4,     %rdx;					\
	/* rax = &stack_entry */	 			\
	addq    %rdx,   %rax;					\
	/* store cap no and sp */				\
	movq    $0x0123456789abcdef, %rdx; 			\
	movq    %rdx, (%rax); 			\
	movq    %rsp,   8(%rax); 				\
	/* increment tos */					\
	movq    (%rcx), %rax;					\
	addq    $1, %rax;					\
	movq    %rax, (%rcx);		

/* r13 = tid, r14 = isb ptr */
#define COS_ULINV_POP_INVSTK					\
	movq    %r14,   %rdx;					\
	movq    %r13,   %rax;					\
	/* get perthread invstack */				\
	shlq    $7,     %rax;    				\
	addq    %rdx,   %rax; 					\
	/* rcx = &stack->top */	 				\
	movq    %rax,   %rcx;					\
	movq    (%rcx), %rdx;					\
	shlq    $4,     %rdx;					\
	/* rax = &stack_entry */	 			\
	addq    %rdx,   %rax;					\
	/* restore sp */					\
	movq    8(%rax),%rsp; 					\
	/* decrement tos */					\
	movq    (%rcx), %rax;					\
	subq    $1, %rax;					\
	movq    %rax, (%rcx);		


/* clang-format on */

#endif
