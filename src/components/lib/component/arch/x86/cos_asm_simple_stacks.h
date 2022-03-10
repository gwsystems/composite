#ifndef COS_ASM_SIMPLE_STACKS_X86_H
#define COS_ASM_SIMPLE_STACKS_X86_H

/* clang-format off */
#define COS_ASM_GET_STACK_BASIC             \
	movl $cos_static_stack, %esp;	    \
	movl %eax, %edx;		    \
	andl $0xffff, %eax;		    \
	shl $MAX_STACK_SZ_BYTE_ORDER, %eax; \
	addl %eax, %esp;		    \
	shr $MAX_STACK_SZ_BYTE_ORDER, %eax; \
	shr $16, %edx;			    \
	pushl %edx;			    \
	pushl %eax;

#define COS_ASM_GET_STACK       \
	COS_ASM_GET_STACK_BASIC \
	pushl $0;

#define COS_ASM_GET_STACK_INVTOKEN \
	COS_ASM_GET_STACK_BASIC    \
	pushl %ecx;

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

/* clang-format on */

#endif
