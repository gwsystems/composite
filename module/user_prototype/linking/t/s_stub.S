#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.data
/* 4 4K stacks */
.globl nil
nil:
	.long 4
.align 32
.globl cos_static_stack
cos_static_stack:
	.rep 4096
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
	addl $4092, %esp
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call spd1_fn
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
