/* Async inv stub. */
	
.globl cos_ainv_entry
.type  cos_ainv_entry, @function
.align 16
cos_ainv_entry:
	/* save regs */
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %edx
	pushl %ebp

	/* arguments */
	movl %esp, %ebx
	addl $24, %ebx
	push %ebx // second arg: params

	/* typeof(%eax) = (usr_inv_cap*) */
	push %eax // first arg: usr_inv_cap

	call cos_async_inv
	addl $8, %esp

	/* Return value is the acap we want to invoke. 0 means no need
	to invoke. (i.e. server upcall thread active) */
	cmpl $0, %eax // invoke acap to send ipi or not?
	je ainv_ret

	/* Load the arguments. We need them if static cap is returned. */
	movl 24(%esp), %ebx
	movl 28(%esp), %esi
	movl 32(%esp), %edi
	movl 36(%esp), %edx

	/* invoke acap %eax */
	movl %esp, %ebp /* save the stack */
	movl $ainv_ret, %ecx /* save the return ip */

	sysenter 

ainv_ret:
	popl %ebp
	popl %edx
	popl %edi
	popl %esi
	popl %ebx

	ret
