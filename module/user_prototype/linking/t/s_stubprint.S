#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl printstr_inv
.type  printstr_inv, @function
.align 16
printstr_inv:
	movl $cos_static_stack, %esp
	shl $12, %eax /* 4K stacks */
	addl %eax, %esp
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call printstr

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl print_vals_inv
.type  print_vals_inv, @function
.align 16
print_vals_inv:
	movl $cos_static_stack, %esp
	shl $12, %eax /* 4K stacks */
	addl %eax, %esp

	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	call print_vals
	/* addl $16, %esp <- no need to clean up */
	
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
