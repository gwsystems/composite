#ifndef COS_ASM_SIMPLE_STACKS_H
#define COS_ASM_SIMPLE_STACKS_H

#ifndef MAX_STACK_SZ_BYTE_ORDER
#error "Missing MAX_STACK_SZ_BYTE_ORDER, try including consts.h"
#endif

#define COS_ASM_GET_STACK                     \
	movl $cos_static_stack, % esp;        \
	movl % eax, % edx;                    \
	andl $0xffff, % eax;                  \
	shl  $MAX_STACK_SZ_BYTE_ORDER, % eax; \
	addl % eax, % esp;                    \
	shr $MAX_STACK_SZ_BYTE_ORDER, % eax;  \
	shr $16, % edx;                       \
	pushl % edx;                          \
	pushl % eax;

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

#endif
