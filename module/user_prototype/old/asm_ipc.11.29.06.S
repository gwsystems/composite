#include <consts.h>
#define PROTOTYPE
		
/* definitions */
IPRETURN = 16 /* the offset on the stack to the fn return address */
SFRAMEIP = 12
SFRAMESP = 8
	
.text

/* FIXME: esi not really needed, use ebp instead on server->client xition */
	
/* stubs for invocation with asymmetric trust */
.globl ipc_invoke_spd
.align 16
ipc_invoke_spd:	
	movl 4(%esp), %eax
	jmp client_marshal

.globl client_marshal
.align 4
client_marshal:
	pushl %ebp
	pushl %ecx
	pushl %edx
	pushl %esi
	movl IPRETURN(%esp), %esi /* save the return ip */
	movl %esp, %ebp

#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp ipc_syscall
#else
	sysenter
#endif
	
/* delivered to the server service */
.globl server_unmarshal
.align 4
server_unmarshal:
	popl %esi
	popl %edx
	popl %ecx
	popl %ebp
	movl $server_ret_marshal, (%esp)
	jmp  *%eax

/* and returning from that server service */
.globl server_ret_marshal
.align 4
server_ret_marshal:
	pushl $0 /* retaddr */
	pushl %ebp
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %eax, %esi
	movl $0, %eax /* ret capability */
#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp ipc_syscall
#else
	sysenter
#endif
	
.globl client_ret_unmarshal
.align 4
client_ret_unmarshal:
	/* replace the correct return address */
	movl %esi, IPRETURN(%esp)
	popl %esi
	popl %edx
	popl %ecx
	popl %ebp
	ret
	

/* kernel-side */
ipc_syscall:
	cmpl $0, %eax /* do we have a return capability */
	jz ret_cap
server_inv:
	pushl %ebp
	pushl %esi /* ip */
	pushl %ebp /* sp */
	pushl %eax /* cap */
	pushl current_thread
	call ipc_walk_static_cap
	addl $16, %esp
	/* ret fn in eax */
	popl %ebp
	
#ifdef PROTOTYPE
	movl %ebp, %esp
	jmp server_unmarshal
#else
	movl %ebp, %ecx
	movl server_unmarshal, %edx
	sysexit 
#endif
	
.align 4	
ret_cap:
	/* ok, this is all quite ugly, and will go away once I make the stack 
	 * frame alignments available in asm */
	pushl %esi /* user-level return value */
	
	pushl current_thread
	call thd_invocation_pop
	addl $4, %esp

	movl SFRAMESP(%eax), %ecx
	movl SFRAMEIP(%eax), %esi
	popl %eax /* user-level ret val */

#ifdef PROTOTYPE
	movl %ecx, %esp
	jmp client_ret_unmarshal
#else
	movl client_ret_unmarshal, %edx
	sysexit 
#endif
