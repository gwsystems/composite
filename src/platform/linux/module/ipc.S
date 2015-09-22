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
	sti ; sysexit

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
	/* room for the inv_ret_struct (thdid, spdid, si, di, bp from dx) */
	pushl %edx /* cdecl makes it legal to use edx in called fn: save it */
	pushl %edi
	pushl %esi
	subl $8, %esp
	pushl %esp /* inv_ret_struct* */
	/* invoke ipc_walk_static_cap */
	pushl %ecx /* ip */
	pushl %ebp /* sp */
	pushl %eax /* cap */
	call ipc_walk_static_cap
	/* %eax = handler fn in server */
	cmpl $0, %eax /* error? */
	jz composite_call_err
	
	addl $16, %esp /* get back on inv_ret_struct */
	movl %eax, %edx

	/* restore possibly modified registers from inv_ret_struct */
	popl %eax
	popl %ecx
	popl %esi
	popl %edi
	popl %ebp /* 4th argument comes from edx, now put in ebp */

	RET_TO_USER_NOSEG
		
.globl composite_ret_ipc
.align 16
composite_ret_ipc:	 /*ret_cap:*/
	pushl %edi
	pushl %esi
	pushl %ecx /* user-level return value */

	pushl $0
	pushl %esp /* ptr to space we just made for return val */
	call pop
	cmpl $0, %eax /* error? */
	jz ipc_ret_slowpath
	addl $8, %esp

	/* eax contains ptr to invocation_frame */
	/* FIXME: this is brain-damaged, should allocate ret area on stack instead of messing with invocation stack frame... Saved for now by the non-preemption thing and fact that thread is only modified one one core at a time. */
	movl SFRAMESP(%eax), %ecx
	movl SFRAMEIP(%eax), %edx
	popl %eax /* user-level ret val */
	popl %esi
	popl %edi
	
	RET_TO_USER_NOSEG

ipc_ret_slowpath:
	addl $4, %esp
	popl %eax /* regs to restore */

	cmpl $0, %eax
	ja ret_from_preemption
		
/* global so that can see it in error reports if we crash */
.globl ipc_ret_err 
.align 16
ipc_ret_err:
	ERROR_OUT

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
	
	pushl %ebp /* user-sp */
	pushl %ecx /* user-ip */
	
	pushl %eax /* cap */

	/* invoke walk_async_cap */
	call walk_async_cap

	addl $4, %esp

	popl %edx /* user-ip from above */
	popl %ecx /* and the sp */

	RET_TO_USER
//	
#define COS_SYSCALL_EDX 20 /* Don't forget space for the return address */
#define COS_SYSCALL_ECX 24

.globl cos_syscall_ainv_wait
.align 16
cos_syscall_ainv_wait:
	LOAD_CURR_THD_EAX;
	
	addl $THD_REGS, %eax    /* offsetof(struct thread, regs) */

	movl $0, PT_EAX(%eax)   /* return 0 when we return from this thread */
	movl %ebp, PT_ECX(%eax) /* save the return sp, so that sysenter will jump to it */
	movl %ebp, PT_OLDESP(%eax)
	movl %ecx, PT_EDX(%eax) /* save the return address (so that sysexit will return to it) */
	movl %ecx, PT_EIP(%eax)

	pushl $0   /* make room for ret val */
	pushl %esp /* argument to track if the thread to return to was preempted or not */
	pushl %ebx /* acap */
	pushl %edx /* this spd_id */
	call cos_syscall_ainv_wait_cont
	addl $12, %esp
	popl %ebx

	cmpl $0, %ebx /* was the thread preempted or not? */
	jne ret_from_preemption
	/* restore registers from previous yield, regs in eax */
	jmp 1f

/* FIXME: this is all superfluous: we save/restore all regs when in the 
   non-preemption case, user-level saves most of them anyway because of 
   the clobber list.  Lets just let user-level do it, and be done. */
.globl cos_syscall_switch_thread
.align 16
cos_syscall_switch_thread:
	/* can use edx to pass in conditional for if we should save regs or not and term thread */
	LOAD_CURR_THD_EAX;
	
	addl $THD_REGS, %eax    /* offsetof(struct thread, regs) */

	movl $0, PT_EAX(%eax)   /* return 0 when we return from this thread */
	movl %ebx, PT_EBX(%eax)
	movl %ebp, PT_ECX(%eax) /* save the return sp, so that sysenter will jump to it */
	movl %ebp, PT_OLDESP(%eax)
	movl %ecx, PT_EDX(%eax) /* save the return address (so that sysexit will return to it) */
	movl %ecx, PT_EIP(%eax)

	/* save volatile registers, as defined by cdecl (including ebx above) */
	movl %esi, PT_ESI(%eax)
	movl %edi, PT_EDI(%eax)
	movl %ebp, PT_EBP(%eax)

	pushl $0   /* make room for ret val */
	pushl %esp /* argument to track if the thread to return to was preempted or not */
	pushl %esi /* flags */
	pushl %ebx /* thd_id */
	pushl %edx /* this spd_id */
	call cos_syscall_switch_thread_cont
	addl $16, %esp
	popl %ebx

	cmpl $0, %ebx /* was the thread preempted or not? */
	jne ret_from_preemption
1:	
	/* restore registers from previous yield, regs in eax */
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
	
	RET_TO_USER

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
	
.globl cos_syscall_upcall
.align 16
cos_syscall_upcall:
	pushl $0
	pushl %esp
	pushl %edi
	pushl %esi
	pushl %ebx /* spd_id to upcall to */
	pushl %edx /* this spd_id */
	call cos_syscall_upcall_cont
	addl $20, %esp
	popl %ebx  /* regs */

	cmpl $-1, %eax
	je   upcall_err

	movl PT_EDX(%ebx), %ecx
	movl %ecx, (COS_SYSCALL_EDX)(%esp)
	movl PT_ECX(%ebx), %ecx
	movl %ecx, (COS_SYSCALL_ECX)(%esp)
	/* Pass upcall arguments. */
	movl PT_ESI(%ebx), %esi
	movl PT_EDI(%ebx), %edi
	movl PT_EBX(%ebx), %ebx
	/* movl %ebx, (COS_SYSCALL_EDX)(%esp)*/
	/* eax remains as the thread id + cpuid */
upcall_err:	
	ret

	; 	pushl $0
; 	pushl %esp
; 	pushl %ebx /* spd_id to upcall to */
; 	pushl %edx /* this spd_id */
; 	call cos_syscall_upcall_cont
; 	addl $12, %esp
	
; 	popl %ebx
; 	movl %ebx, (COS_SYSCALL_EDX)(%esp)
; 	/* eax remains as the thread id */
	
; 	ret

	
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
	pushl %ebp /* user-sp */
	pushl %ecx /* user-ip */
	
	/* arguments -- changing layout here requires changing COS_SYSCALL_E(C|D)X */
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %edx
	shr $COS_SYSCALL_OFFSET, %eax /* impossible for value to be greater than the max allowed here because of filtering in kern_entry.S */
	call *cos_syscall_tbl(,%eax,4)
	addl $16, %esp /* esi, edi, and ebx not trashed because of cdecl conventions, and edx's value doesn't really matter here */

	popl %edx /* user-ip from above */
	popl %ecx /* and the sp */

	RET_TO_USER

.align 16
composite_call_err:	
	addl $4, %esp
	movl $-1, %eax
	popl %ecx
	/* perhaps more appropriate to trap to exception handler */
	popl %edx
	popl %ebp
	addl $4, %esp

	RET_TO_USER
	
/* global so that can see it in error reports if we crash */
.globl composite_ret_err 
.align 16
composite_ret_err:
	/* should invoke fault handler, but for now, kill our thread
	by causing error (remember that pop just returned 0 in eax) */
	/*	sti*/
	ERROR_OUT
