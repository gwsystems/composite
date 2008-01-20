#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */

.globl foo_inv
.type  foo_inv, @function
.align 16
foo_inv:
	movl $cos_static_stack, %esp
	movl %eax, %ebp
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp

	pushl %edi
	pushl %esi
	pushl %ebx
/*	pushl %eax*/
	call foo
	/* addl $16, %esp <- no need to clean up */
	
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
