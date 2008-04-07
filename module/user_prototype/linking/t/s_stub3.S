#include <cos_asm_stacks.h>
	
#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */

.globl bar_inv
.type  bar_inv, @function
.align 16
bar_inv:
	COS_ASM_GET_STACK
	
	pushl %edi
	pushl %esi
	pushl %ebx
/*	pushl %eax*/
	call bar
	/* addl $16, %esp <- no need to clean up */
	
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
