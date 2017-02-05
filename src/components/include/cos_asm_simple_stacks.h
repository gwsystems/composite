#ifndef COS_ASM_SIMPLE_STACKS_H
#define COS_ASM_SIMPLE_STACKS_H

#define COS_ASM_GET_STACK                   \
	movl $cos_static_stack, %esp;	    \
	movl %eax, %edx;		    \
	andl $0xffff, %eax;		    \
	shl $12, %eax;                      \
	addl %eax, %esp;		    \
	shr $12, %eax;			    \
	shr $16, %edx;			    \
	pushl %edx;			    \
	pushl %eax;

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

#endif
