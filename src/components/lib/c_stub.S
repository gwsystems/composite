#define RET_CAP ((1<<20)-1)
#define IPRETURN 4
#define CAPNUM 12

#ifdef NIL
.globl SS_ipc_client_marshal
.type  SS_ipc_client_marshal, @function
.align 16
SS_ipc_client_marshal:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */

	pushl %ebx
	pushl %esi
	pushl %edi
	
	pushl %ebp
/* X: Don't need to save these as C code should do this automatically
   kernel and server, UNLESS we don't trust the server.  Good way to pass args.
	pushl %ebx
	pushl %esi
	pushl %edi
*/
/*	movl IPRETURN(%esp), %edx */ /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $SS_ipc_client_ret_unmarshal, %ecx /* save the return ip */

	sysenter 	
#endif
	
.globl SS_ipc_client_marshal_args
.type  SS_ipc_client_marshal_args, @function
.align 16
SS_ipc_client_marshal_args:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */
	
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %edx
	
	movl 20(%esp), %ebx
	movl 24(%esp), %esi
	movl 28(%esp), %edi
	movl 32(%esp), %edx
	
	pushl %ebp
/*	movl IPRETURN(%esp), %edx */ /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $SS_ipc_client_ret_unmarshal, %ecx /* save the return ip */

	sysenter 
			
.globl SS_ipc_client_ret_unmarshal
.align 4
SS_ipc_client_ret_unmarshal:
	/* replace with the correct return address */
/*	movl %ebp, IPRETURN(%esp)*/ /* user-defined value */
/* See X comment above
	popl %edi
	popl %esi
	popl %ebx
*/
	popl %ebp

	popl %edx
	popl %edi
	popl %esi
	popl %ebx
	
	ret

#ifdef NIL
.globl SS_ipc_client_marshal_saveregs
.type  SS_ipc_client_marshal_saveregs, @function
.align 16
SS_ipc_client_marshal_saveregs:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */
/*	shll $0x10,%eax */
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
/*	movl IPRETURN(%esp), %edx */ /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $SS_ipc_client_ret_unmarshal_saveregs, %ecx /* save the return ip */

	sysenter 
	
.globl SS_ipc_client_ret_unmarshal_saveregs
.align 4
SS_ipc_client_ret_unmarshal_saveregs:
	/* replace with the correct return address */
/*	movl %ebp, IPRETURN(%esp)*/ /* user-defined value */
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
#endif
