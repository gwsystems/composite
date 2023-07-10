#include <consts.h>

.align COS_STACK_SZ;
.globl cos_static_stack;
cos_static_stack:
	.rep 20971520;
	.byte 0	;
	.endr ;
.globl cos_static_stack_end;
cos_static_stack_end:

.text;
.align 16;
.globl custom_acquire_stack;
custom_acquire_stack:
	movabs $cos_static_stack, %rsp;
	jmpq *%rcx;
