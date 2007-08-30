.data
.globl intermed_cap
intermed_cap:	
.long 0

#define RET_CAP ((1<<16)-1)
.text
.globl invokee
invokee:
	movl $RET_CAP, %eax
	movl intermed_cap, %ecx
/*	movl %esp, %ecx*/
	sysenter

/* doesn't work: */
.align 16
.globl cos_intermediate
cos_intermediate:
	movl %esp, %ebp
	movl $after_intermed_inv, %ecx
	movl intermed_cap, %eax
	shl $16, %eax
	sysenter
after_intermed_inv:
	movl %eax, %ecx
	movl $RET_CAP, %eax

#define IPRETURN 4

.globl SS_ipc_client_marshal
.align 16
SS_ipc_client_marshal:
	/*movl CAPNUM(%eax), %eax*/ /* typeof(%eax) = (usr_inv_cap*) */
	pushl %ebp
/*	pushl %ecx
	pushl %edx */

	movl IPRETURN(%esp), %edx /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $SS_ipc_client_ret_unmarshal, %ecx /* save the return ip */

	sysenter 
	
.globl SS_ipc_client_ret_unmarshal
.align 4
SS_ipc_client_ret_unmarshal:
	/* replace with the correct return address */
	movl %ebp, IPRETURN(%esp)
/*	popl %edx
	popl %ecx*/
	popl %ebp
	ret
	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl SS_ipc_server_unmarshal
.align 16
SS_ipc_server_unmarshal:
	movl %ebp, %esp
	/* pops here to support stack sharing */
/*	popl %edx
	popl %ecx*/
	popl %ebp
	addl $4, %esp
/*	call *%eax*/
	call c_invokee
	pushl $0 /* space for the retaddr */
	pushl %ebp
/*	pushl %ecx
	pushl %edx*/
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
