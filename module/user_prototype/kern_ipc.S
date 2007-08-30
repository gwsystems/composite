/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#include <consts.h>
#include <asm_ipc_defs.h>
#define PROTOTYPE		
/* Code */
		
.text

/* 
 * FIXME: 
 * 1) cdecl calling conventions are that eax/ecx/edx are for free use,
 *    so probably don't need to save them all 
 */

/*
 * GCC will compile
 * "return fn_call();"
 * to
 * "leave
 *  jmp fn_call"
 * to take advantage of the tail-call.
 *
 * The problem arises when the return pointer now points to a
 * return address in another component.  This problem should only
 * manefest itself when we are invoking a component from a test
 * platform (like cos_loader), and should not present itself
 * after that as all server stubs will provide correct return
 * addresses (given cdecl).  The root cause of this issue is not
 * known, but those are the symptoms.
 */
	

	
/* SS stands for shared-stack: This is the invocation case, regardless
 * of stub code or function prototypes, that will work when
 * caller and receiver (both cdecl conformant) share a stack. 
 */
	
/* %eax holds a ptr to the usr_inv_cap so we can get the real cap no. */
.globl SS_ipc_client_marshal
.align 16
SS_ipc_client_marshal:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */
	pushl %ebp
/*	pushl %ecx
	pushl %edx */

	movl IPRETURN(%esp), %edx /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $SS_ipc_client_ret_unmarshal, %ecx /* save the return ip */

#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp kernel_ipc_syscall
#else
	sysenter
#endif
	
.globl SS_ipc_client_ret_unmarshal
.align 4
SS_ipc_client_ret_unmarshal:
	/* replace the correct return address */
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
	call *%eax
	pushl $0 /* space for the retaddr */
	pushl %ebp
/*	pushl %ecx
	pushl %edx*/
	movl %eax, %ecx
	movl $0, %eax /* return capability */
#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp kernel_ipc_syscall
#else
	sysenter
#endif


.globl DS_ipc_client_marshal
.align 16
DS_ipc_client_marshal:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */
	pushl %ebp
/*	pushl %ecx
	pushl %edx */

	/*movl IPRETURN(%esp), %edx*/ /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $DS_ipc_client_ret_unmarshal, %ecx /* save the return ip */

#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp kernel_ipc_syscall
#else
	sysenter
#endif
	
.globl DS_ipc_client_ret_unmarshal
.align 4
DS_ipc_client_ret_unmarshal:
	/* replace the correct return address */
	/*movl %ebp, IPRETURN(%esp)*/
/*	popl %edx
	popl %ecx*/
	popl %ebp
	ret
	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl DS_ipc_server_unmarshal
.align 16
DS_ipc_server_unmarshal:
	movl cos_static_stack, %esp
	/* pops here to support stack sharing */
/*	popl %edx
	popl %ecx*/
	/*popl %ebp
	addl $4, %esp*/
	call *%eax
	/*pushl $0*/ /* space for the retaddr */
	/*pushl %ebp*/
/*	pushl %ecx
	pushl %edx*/
	movl %eax, %ecx
	movl $0, %eax /* return capability */
#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp kernel_ipc_syscall
#else
	sysenter
#endif
	
	
/* kernel-side */

/*****************************/	
/*** FIXME: error checking ***/
/*****************************/	
	
kernel_ipc_syscall:
	cmpl $0, %eax /* do we have a return capability */
	jz ret_cap
server_inv:
	pushl %ebp

	/* invoke ipc_walk_static_cap */
	pushl %edx /* usr_def */
	pushl %ecx /* ip */
	pushl %ebp /* sp */
	pushl %eax /* cap */
	pushl current_thread
	call ipc_walk_static_cap
	addl $20, %esp
	/* %eax = handler fn in server */

	/* FIXME: pass id in ebp? */
#ifdef PROTOTYPE
	popl %ebp /* invoking component's user stack */
	/* movl INV_STK_START+INV_STK_SIZE-4, %esp */
	jmp DS_ipc_server_unmarshal
#else
	/* movl INV_STK_START+INV_STK_SIZE-4, %ecx */
	/*popl %ecx*/ /* user stack */
	movl DS_ipc_server_unmarshal, %edx
	/* sti */
	sysexit 
#endif
	
.align 16
ret_cap:
	pushl %ecx /* user-level return value */

	pushl current_thread
	call pop
	addl $4, %esp

	/* eax contains ptr to invocation_frame */
	movl SFRAMESP(%eax), %ecx
	movl SFRAMEUSR(%eax), %ebp
	movl SFRAMEIP(%eax), %edx
	popl %eax /* user-level ret val */

#ifdef PROTOTYPE
	movl %ecx, %esp
	jmp *%edx /*ipc_client_ret_unmarshal*/
#else
	movl *%edx, %edx
	sysexit 
#endif
