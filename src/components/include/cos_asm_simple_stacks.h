
#ifndef COS_ASM_SIMPLE_STACKS_H
#define COS_ASM_SIMPLE_STACKS_H

#define COS_ASM_GET_STACK                   \
	movl $cos_static_stack, %esp;	    \
	shl $9, %eax;			    \
	addl %eax, %esp;

#define COS_ASM_RET_STACK


#endif
