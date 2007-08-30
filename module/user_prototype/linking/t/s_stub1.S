#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.data
/* 8 4K stacks */
.globl nil
nil:
	.long 4
.align 32
first_stack:
	.rep 1024
	.long 4
	.endr
.globl cos_static_stack
cos_static_stack:
	.rep 8193
	.long 4
	.endr

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl spd1_inv
.type  spd1_inv, @function
.align 16
spd1_inv:
	movl $cos_static_stack, %esp
	movl %eax, %ebp
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp

	pushl %eax /* thread id */
	pushl %ecx /* data region ptr */	
	call spd1_fn
	/* addl $8, %esp <- dont need to clean the stack */

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
