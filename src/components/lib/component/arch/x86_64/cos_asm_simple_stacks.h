#ifndef COS_ASM_SIMPLE_STACKS_X86_H
#define COS_ASM_SIMPLE_STACKS_X86_H

/* clang-format off */
#define COS_ASM_GET_STACK_BASIC             \
	mov $cos_static_stack, %rsp;	    \
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
	push %rcx;

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

/* clang-format on */

#endif
