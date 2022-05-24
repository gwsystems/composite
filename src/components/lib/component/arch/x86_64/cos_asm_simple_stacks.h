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

/* r13 = tid, r14 = isb ptr -> rcx = &stack */
#define COS_ULINV_GET_INVSTK					\
	movabs	$ULK_BASE_ADDR, %r14;				\
	/* access IAT to get thd isb index */       		\
	movw	(%r14, %r13, 2), %ax;				\
	/* use index to get perthread invstk */			\
	addq    $PAGE_SIZE, %r14;				\
	movq    %r14,   %rdx;					\
	/* get perthread invstack */				\
	shlq    $7,     %rax;    				\
	addq    %rdx,   %rax; 					\
	/* rcx = &stack->top */	 				\
	movq    %rax,   %rcx;					\



/* rcx = &stack */
#define COS_ULINV_PUSH_INVSTK					\
	movq    (%rcx), %rdx;					\
	addq    $1,     %rdx;					\
	shlq    $4,     %rdx;					\
	/* rax = &stack_entry */	 			\
	addq    %rdx,   %rax;					\
	/* store cap no and sp */				\
	movq    $0x0123456789abcdef, %rdx; 			\
	movq    %rdx, (%rax); 					\
	movq    %rsp, 8(%rax); 					\
	/* increment tos */					\
	movq    (%rcx), %rax;					\
	addq    $1, %rax;					\
	movq    %rax, (%rcx);		

/* r13 = tid, r14 = isb ptr */
#define COS_ULINV_POP_INVSTK					\
	movq    (%rcx), %rdx;					\
	shlq    $4,     %rdx;					\
	/* rax = &stack_entry */	 			\
	addq    %rdx,   %rax;					\
	/* restore sp */					\
	movq    8(%rax),%rsp; 					\
	/* decrement tos */					\
	movq    (%rcx), %rdx;					\
	subq    $1, %rdx;					\
	movq    %rdx, (%rcx);					\


/* clang-format on */

#endif
