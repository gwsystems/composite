#include <cos_asm_stacks.h>
	
#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl spd1_inv
.type  spd1_inv, @function
.align 16
spd1_inv:
	COS_ASM_GET_STACK
	
	pushl %eax /* thread id */
	pushl %ecx /* data region ptr */	
	call spd1_fn
	/* addl $8, %esp <- dont need to clean the stack */

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

