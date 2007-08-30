#define RET_CAP ((1<<16)-1)
#define IPRETURN 4
#define CAPNUM 12


.globl SS_ipc_client_marshal
.type  SS_ipc_client_marshal, @function
.align 16
SS_ipc_client_marshal:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */
	shll $0x10,%eax
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
