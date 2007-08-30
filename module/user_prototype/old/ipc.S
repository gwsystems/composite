/**
 * See ipc.txt for a complete description of this IPC mechanism.
 */

.ipc_text
.globl client_ipc
.align 4
	
#define PAGE_MASK 0xFFFFF000
	
client_ipc:
	movl *%esp, %esp /* first word in thread_t must be the stack ptr */
	cmpl $0, %eax /* is a server returning? */
	jz server_ret_ipc

	/* populate the kernel stack frame for this ipc */
	subl $THD_STACK_FRAME_SIZE, %esp
	movl %esi, THD_IP(%esp) /* eip of return address of client */
	movl %ebp, THD_ESP(%esp) /* esp of client */
	/* subl $8, %esp */ /* place-holder for spd pointer, and alignment for next stack frame */
	movl THD_STACK_FRAME_SIZE(%esp), %ebp 
	/* get the current spd ptr. */
	movl THD_SPD_PTR(%ebp), %esi 
	
	/* we need to look up in the cap table the service protection domain info */
	movl *(%esi), %esi /* deref the pointer to get the spd */
	cmpl SPD_CAP_TBL_SZ(%esi), %eax /* is our capability within range? */
	jae error_bad_cap

	/*	addl $THD_STACK_FRAME_SIZE,  */
	 /* guessing the syntax... */
	movl %esi(,%eax,$CAP_SIZE), %eax

	/* get the entry instruction and the spd of the service */
	movl CAP_ENTRY_INST(%eax), %ecx
	movl CAP_SPD(%eax), %edx

	movl %edx, THD_SPD_PTR(%ebp) /* populate the current spd in our stack frame. */

	pushl %ecx /* save the return address */
	pushl %cr3
	pushl SPD_LOCATION_INFO+%esi

	call *(SPD_OPEN_FN_PTR(%esi)) /* do in assembly later...? */

	addl $8, %esp
	/* retrieve the entry point */
	popl %eax

	movl THD_SP(%ebp), %edx /* read sysexit to make sure edx is right */
	
	andl $PAGE_MASK, %ebp
	addl $THD_STACK_FRAME_SIZE, THD_STACK_PTR(%ebp) /* add our frame officially to the stack */

	movl server_unmarshal, %ecx /* read sysexit to make sure ecx is right */

	sysexit

.globl server_ret_ipc
.align 4
server_ret_ipc:
	movl %esi, %eax
	movl THD_STACK_FRAME_SIZE(%esp), %ebp

	movl %esp, %ecx
	andl $PAGE_MASK, %ecx
	subl $THD_STACK_FRAME_SIZE, THD_STACK_PTR(%ecx)
	
	movl THD_SPD_PTR(%ebp), %ecx
	movl *%ecx, %ecx

	pushl %cr3
	pushl %ecx+SPD_LOCATION_INFO

	call *(SPD_CLOSE_FN_PTR(%ecx))

	movl THD_IP(%ebp), %esi
	movl client_ret_unmarshal, %ecx
	movl THD_SP(%ebp), %edx

	sysexit
		
error_bad_cap:
	/* kill client */
	
.user_ipc_stubs
#define ORIG_EIP_RETURN 16
#define POST_RPC_EIP_RET 12
		
/* a synchronous invocation made by the client */
.globl client_marshal
.align 4
client_marshal:
	pushl %ebp
	pushl %ecx
	pushl %edx
	pushl %esi
	movl EIP_RETURN(%esp), %esi /* save the return eip */
	movl %esp, %ebp
	sysenter
/* delivered to the server service */
.globl server_unmarshal
.align 4
server_unmarshal:
	popl %esi
	popl %edx
	popl %ecx
	popl %ebp
	popl server_ret_marshal
	jmp  *%eax

/* and returning from that server service */
.globl server_ret_marshal
.align 4
server_ret_marshal:
	pushl $0
	pushl %ebp
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %eax, %esi
	movl $0, %eax
	sysenter
.globl client_ret_unmarshal
.align 4
client_ret_unmarshal:
	movl %esi, POST_RPC_EIP_RET(%esp)
	popl %esi
	popl %edx
	popl %ecx
	popl %ebp
	ret
	
