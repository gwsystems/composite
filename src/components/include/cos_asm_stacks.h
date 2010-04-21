#ifndef COS_ASM_STACKS_H
#define COS_ASM_STACKS_H

/*
 * TODO: this could all be done without using eax.  If the thdid was
 * pass in as ecx (thus esp as well given sysexit rules), these
 * computations would require 2 instructions only manipulating esp.
 * However, we would now need to pass in the spdid in eax, as ecx
 * (where it is now) would be overloaded.
 */
#define COS_ASM_GET_STACK                   \
	movl $cos_static_stack, %esp;	    \
	shl $11, %eax;			    \
	addl %eax, %esp;

#define COS_ASM_RET_STACK

#endif
