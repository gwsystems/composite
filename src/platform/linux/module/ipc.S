/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 */

/* push a known value onto the stack, so that we can tell, if we get
   an interrupt after the sti, that we are in a composite IPC
   call. See comment in hijack.c:get_user_regs.  Essentially, if we 
   get an interrupt, this makes the stack look like if user-level
   were preempted (in terms of register contents) */

#define RET_TO_USER_NOSEG \
	pushl $0; \
	pushl $0; \
	sti; 	  \
	sysexit

//#define RET_TO_USER RET_TO_USER_NOSEG
#define RET_TO_USER \
	pushl %edx		;\
	movl $(__USER_DS), %edx ;\
	movl %edx, %fs		;\
	popl %edx		;\
	RET_TO_USER_NOSEG

/* null ptr deref */
#define ERROR_OUT \
	movl $0, %eax ; \
	movl (%eax), %eax

/* Loads current thread structure to eax register. ECD and EDX can be
 * clobbered, as per the calling convention. */
#define LOAD_CURR_THD_EAX \
	pushl %ecx		   ;\
	pushl %edx		   ;\
	call core_get_curr_thd_asm ;\
	popl %edx		   ;\
	popl %ecx		  

	
#define THD_REGS 8 /* offsetof(struct thread, regs) */
		
#include <asm/asm-offsets.h>
#include <asm/segment.h> /* __USER_(DS|CS) */
#include "../../../kernel/include/asm_ipc_defs.h"

.text

kernel_ipc_syscall:
	cmpl $0, %eax /* do we have a return capability */
	jz composite_ret_ipc


	//do we need to save ip and sp in pt_regs?
	;; subl $4, %esp;  \
	;; pushl %ecx;     \
	;; subl $8, %esp;  \
	;; pushl %edx;  	\
	;; subl $20, %esp; \

#define SAVE_REGS_GENERAL 	\
	subl $40, %esp; 	\
	pushl %eax;		\
	pushl %ebp;		\
	pushl %edi;		\
	pushl %esi;		\
	pushl %edx;		\
	pushl %ecx;		\
	pushl %ebx

#define SAVE_REGS_SEG 		\
	subl $8, %esp; 		\
	pushfl;			\
	subl $28, %esp;		\
	pushl %eax;		\
	pushl %ebp;		\
	pushl %edi;		\
	pushl %esi;		\
	pushl %edx;		\
	pushl %ecx;		\
	pushl %ebx

/* When return to user: ecx => sp, edx => ip. */
#define RESTORE_REGS_GENERAL	\
	popl %ebx;		\
	popl %ecx;		\
	popl %edx;		\
	popl %esi;		\
	popl %edi;		\
	popl %ebp;		\
	popl %eax;		\
	addl $40, %esp

#define RESTORE_REGS_SEG	\
	popl %ebx;		\
	popl %ecx;		\
	popl %edx;		\
	popl %esi;		\
	popl %edi;		\
	popl %ebp;		\
	popl %eax;		\
	addl $28, %esp;		\
	popfl;			\
	addl $8, %esp

//	movl $(__USER_DS), %eax;\
//	movl %eax, %ds;		\
//	movl $(__USER_CS), %eax;\
//	movl %eax, %cs;		\

/*
 * Register layout:
 *
 * eax:	which capability, see comment in kern_entry.S:asym_exec_dom_entry
 * ecx:	ip
 * ebp:	sp
 * ebx,esi,edi,edx: arguments
 */
.globl composite_call_ipc
.align 32
composite_call_ipc:	/* server_inv: */
	SAVE_REGS_GENERAL

	pushl %esp /* pt_regs */
	/* invoke ipc_walk_static_cap */
	call ipc_walk_static_cap
	addl $4, %esp

	RESTORE_REGS_GENERAL
	RET_TO_USER_NOSEG
		
.globl composite_ret_ipc
.align 16
composite_ret_ipc:	 /*ret_cap:*/
	SAVE_REGS_GENERAL

	pushl %esp
	call pop
	addl $4, %esp
	
	RESTORE_REGS_GENERAL
	RET_TO_USER_NOSEG

/*
 * Register layout:
 *
 * eax:	which capability, see comment in kern_entry.S:asym_exec_dom_entry
 * ecx:	ip
 * ebp:	sp
 * ebx,esi,edi,edx: arguments
 */
.globl composite_async_call
.align 32
composite_async_call:
	/* set up seg reg */
	pushl %edx
	movl $(__KERNEL_PERCPU), %edx
	movl %edx, %fs
	popl %edx

	SAVE_REGS_GENERAL
	pushl %esp
	/* invoke walk_async_cap */
	call walk_async_cap
	addl $4, %esp

	RESTORE_REGS_GENERAL
	RET_TO_USER
//	
#define COS_SYSCALL_EDX 20 /* Don't forget space for the return address */
#define COS_SYSCALL_ECX 24

#define SP_RET_OFFSET 4
.globl cos_syscall_ainv_wait
.align 16
cos_syscall_ainv_wait:
	movl SP_RET_OFFSET(%esp), %eax
	pushl %eax
	call cos_syscall_ainv_wait_cont
	addl $4, %esp

	testl %eax, %eax
	jne ret_from_preemption_getregs

	ret

.globl cos_syscall_switch_thread
.align 16
cos_syscall_switch_thread:
	movl SP_RET_OFFSET(%esp), %eax
	pushl %eax
	call cos_syscall_switch_thread_cont
	addl $4, %esp

	testl %eax, %eax
	jne ret_from_preemption_getregs

	ret

.align 4
ret_from_preemption_getregs:
	LOAD_CURR_THD_EAX;
	
	addl $THD_REGS, %eax /* offsetof(struct thread, regs) */
ret_from_preemption:
	/* restore from preemption */
	movl PT_EBX(%eax), %ebx
	movl PT_ECX(%eax), %ecx
	movl PT_EDX(%eax), %edx
	movl PT_ESI(%eax), %esi
	movl PT_EDI(%eax), %edi
	movl PT_EBP(%eax), %ebp
	
	pushl $(__USER_DS)
	pushl PT_OLDESP(%eax)
	pushl PT_EFLAGS(%eax)
	pushl $(__USER_CS)
	pushl PT_EIP(%eax)

	movl PT_EAX(%eax), %eax				
	
	iret	

	
.align 4
.globl cos_syscall_buff_mgmt
cos_syscall_buff_mgmt:
	LOAD_CURR_THD_EAX;
	
	pushl %eax              /* save the current thread */
	addl $THD_REGS, %eax    /* offsetof(struct thread, regs) */
	movl %ebp, PT_ECX(%eax) /* save the return sp, so that sysenter will jump to it */
	movl %ecx, PT_EDX(%eax) /* save the return address (so that sysexit will return to it) */
	
	/* still need to pass arguments */
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %edx
	call  cos_syscall_buff_mgmt_cont
	addl  $16, %esp
	popl  %ebx              /* retrieve saved thread */

	pushl %eax
	LOAD_CURR_THD_EAX;
	cmpl %eax, %ebx
	popl %eax
	jne   cos_syscall_interrupted
	
	ret
/* 
 * This is horrible: when we are in buff_mgmt transmitting packets, 
 * we can generate softirqs, in which case we might need to switch 
 * threads here.  See the comment in cosnet_xmit_packet.
 */
cos_syscall_interrupted:
	/* store the return value into the previous thread */
	addl  $THD_REGS, %ebx
	movl  %eax, PT_EAX(%ebx)

	/* get the regs of the new thread, load 'em up, and start 'er flying */
	LOAD_CURR_THD_EAX;
	
	addl $THD_REGS, %eax
	movl PT_EBX(%eax), %ebx
	movl PT_ECX(%eax), %ecx
	movl PT_EDX(%eax), %edx
	movl PT_ESI(%eax), %esi
	movl PT_EDI(%eax), %edi
	movl PT_EBP(%eax), %ebp
	movl PT_EAX(%eax), %eax

	movl %edx, (COS_SYSCALL_EDX)(%esp)
	movl %ecx, (COS_SYSCALL_ECX)(%esp)
	ret
/* end cos_syscall_buff_mgmt */

.globl cos_syscall_idle
.align 16
cos_syscall_idle:
	LOAD_CURR_THD_EAX;
	
	pushl %eax              /* save the current thread */
	addl $THD_REGS, %eax    /* offsetof(struct thread, regs) */
	movl %ebp, PT_ECX(%eax) /* save the return sp, so that sysenter will jump to it */
	movl %ecx, PT_EDX(%eax) /* save the return address (so that sysexit will return to it) */
	
	pushl %edx
	call  cos_syscall_idle_cont
	addl  $4, %esp
	popl  %ebx              /* retrieve saved thread */
	
	pushl %eax
	LOAD_CURR_THD_EAX;
	cmpl %eax, %ebx
	popl %eax
	jne   cos_syscall_interrupted
	ret
	
/*
 * Register Layout (from cos_component.h):
 * eax:	syscall offset, see kern_entry.S: asym_exec_dom_entry
 * edx:	current declared spd id
 * ebx:	first arg
 * esi:	second arg
 * edi:	third arg
 * ecx:	ret ip
 * ebp:	ret esp
 */	
.globl composite_make_syscall
.align 16
composite_make_syscall:
	SAVE_REGS_GENERAL
	
	pushl %esp
	shr $COS_SYSCALL_OFFSET, %eax /* impossible for value to be greater than the max allowed here because of filtering in kern_entry.S */
	call *cos_syscall_tbl(,%eax,4)
	addl $4, %esp

	RESTORE_REGS_GENERAL
	RET_TO_USER
