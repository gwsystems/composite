/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* push a known value onto the stack, so that we can tell, if we get
   an interrupt after the sti, that we are in a composite IPC
   call. See comment in hijack.c:get_user_regs.  Essentially, if we 
   get an interrupt, this makes the stack look like if user-level
   were preempted (in terms of register contents) */
#define RET_TO_USER \
	pushl $0; \
	pushl $0; \
	sti ; sysexit

/* null ptr deref */
#define ERROR_OUT \
	movl $0, %eax ; \
	movl (%eax), %eax

#define THD_REGS 8 /* offsetof(struct thread, regs) */
		
#include <asm/asm-offsets.h>
#include <asm/segment.h> /* __USER_(DS|CS) */
#include "include/asm_ipc_defs.h"

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
	/*pushl %ebp*/ /* user-sp: useful if using same stack */

	pushl %edx /* cdecl makes it legal to use edx in called fn: save it */
	/* room for the thdid and spdid */
	subl $8, %esp
	pushl %esp
	/* invoke ipc_walk_static_cap */
/*	pushl %edx*/ /* usr_def */
	pushl %ecx /* ip */
	pushl %ebp /* sp */
	pushl %eax /* cap */
	pushl current_thread
	call ipc_walk_static_cap
	/* %eax = handler fn in server */
	cmpl $0, %eax /* error? */
	jz composite_call_err
	
/*	addl $24, %esp*/
	addl $20, %esp

	/* these 2 lines (and the pushl ebp at the top) are not needed: can pass data in ebp, ecx, and eax, i.e. for spd defined ptr, thdid/spdid, and data page ptr */
	/* FIXME: pass id in ebp? */
	/*popl %ebp*/ /* invoking component's user stack */ /* can use ebp here to hold more data and pass it to server */
	/*movl %ebp, %ecx*/
	
	/* movl INV_STK_START+INV_STK_SIZE-4, %ecx */
	/*popl %ecx*/ /* user stack */
	/*movl SS_ipc_server_unmarshal, %edx*/
	movl %eax, %edx

	/* thd and data_region */
	popl %eax
	popl %ecx

	popl %ebp /* 4th argument */

	RET_TO_USER
		
.globl composite_ret_ipc
.align 16
composite_ret_ipc:	 /*ret_cap:*/
	pushl %ecx /* user-level return value */

	pushl $0
	pushl %esp /* ptr to space we just made for return val */
	pushl current_thread
	call pop
	cmpl $0, %eax /* error? */
	jz ipc_ret_slowpath
	addl $12, %esp

	/* eax contains ptr to invocation_frame */
	/* FIXME: this is brain-damaged, should allocate ret area on stack instead of messing with invocation stack frame... Saved for now by the non-preemption thing and fact that thread is only modified one one core at a time. */
	movl SFRAMESP(%eax), %ecx
	movl SFRAMEIP(%eax), %edx
	popl %eax /* user-level ret val */

	RET_TO_USER

ipc_ret_slowpath:
	addl $8, %esp
	popl %eax /* regs to restore */

	cmpl $0, %eax
	ja ret_from_preemption
		
/* global so that can see it in error reports if we crash */
.globl ipc_ret_err 
.align 16
ipc_ret_err:
	ERROR_OUT

/* this assumes the stack layout is inherited from composite_make_syscall, and will return directly */
.globl cos_syscall_resume_return
.align 16
cos_syscall_resume_return:
	call cos_syscall_resume_return_cont
	addl $20, %esp /* 4 for ret addr + 16 for args pushed in composite_make_syscall */

	/* see inv.c:cos_syscall_resume_return_cont for ret vals */
	cmpl $1, %eax
	ja ret_from_preemption_getregs
	jb syscall_resume_err

	/* Thread wasn't preempted and voluntarily gave up the cpu, thus return */
	movl $0, %ecx
	jmp composite_ret_ipc

/* global so that can see it in error reports if we crash */
.globl syscall_resume_err 
.align 16
syscall_resume_err:
	/* should invoke fault handler, but for now, kill our thread
	by causing error (remember that pop just returned 0 in eax) */
	/*	sti*/
	ERROR_OUT

#define COS_SYSCALL_EDX 20 /* Don't forget space for the return address */
#define COS_SYSCALL_ECX 24


.globl cos_syscall_brand_upcall
.align 16
cos_syscall_brand_upcall:
	movl current_thread, %eax
	addl $THD_REGS, %eax    /* offsetof(struct thread, regs) */

	movl $0, PT_EAX(%eax)   /* return 0 when we return from this thread */
	movl %ebx, PT_EBX(%eax)
	movl %ebp, PT_ECX(%eax) /* save the return sp, so that sysenter will jump to it */
	movl %ecx, PT_EDX(%eax) /* save the return address (so that sysexit will return to it) */

	/* save volatile registers, as defined by cdecl (including ebx above) */
	movl %esi, PT_ESI(%eax)
	movl %edi, PT_EDI(%eax)

	pushl %esi
	pushl %ebx
	pushl %edx /* this spd_id */
	call cos_syscall_brand_upcall_cont
	addl $12, %esp

	/* eax = thd to continue -- restore registers from previous yield, again */
	movl PT_EBX(%eax), %ebx
	movl PT_ECX(%eax), %ecx
	movl %ecx, (COS_SYSCALL_ECX)(%esp) 
	movl PT_EDX(%eax), %edx
	movl %edx, (COS_SYSCALL_EDX)(%esp) 
	movl PT_ESI(%eax), %esi
	movl PT_EDI(%eax), %edi
	movl PT_EAX(%eax), %eax
		
	ret


	
	
/* FIXME: this is all superfluous: we save/restore all regs when in the 
   non-preemption case, user-level saves most of them anyway because of 
   the clobber list.  Lets just let user-level do it, and be done. */
.globl cos_syscall_switch_thread
.align 16
cos_syscall_switch_thread:
	/* can use edx to pass in conditional for if we should save regs or not and term thread */
	movl current_thread, %eax
	addl $THD_REGS, %eax    /* offsetof(struct thread, regs) */

	movl $0, PT_EAX(%eax)   /* return 0 when we return from this thread */
	movl %ebx, PT_EBX(%eax)
	movl %ebp, PT_ECX(%eax) /* save the return sp, so that sysenter will jump to it */
	movl %ecx, PT_EDX(%eax) /* save the return address (so that sysexit will return to it) */

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
	movl current_thread, %eax
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
.globl cos_switch_error
cos_switch_error:
	movl $-1, PT_EAX(%eax)
	jmp 1b	

.globl cos_syscall_kill_thd
.align 16
cos_syscall_kill_thd:
	/* todo */
	pushl %ebx
	call cos_syscall_kill_thd_cont
	ret

.globl cos_syscall_upcall
.align 16
cos_syscall_upcall:
	pushl $0
	pushl %esp
	pushl %ebx /* spd_id to upcall to */
	pushl %edx /* this spd_id */
	call cos_syscall_upcall_cont
	addl $12, %esp
	
	popl %ebx
	movl %ebx, (COS_SYSCALL_EDX)(%esp)
	/* eax remains as the thread id */
	
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
	pushl %ebp /* user-sp */
	pushl %ecx /* user-ip */
	
	/* arguments -- changing layout here requires changing COS_SYSCALL_E(C|D)X */
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %edx
	shr $16, %eax /* impossible for value to be greater than the max allowed here because of filtering in kern_entry.S */
	call *cos_syscall_tbl(,%eax,4)
	addl $16, %esp /* esi, edi, and ebx not trashed because of cdecl conventions, and edx's value doesn't really matter here */

	popl %edx /* user-ip from above */
	popl %ecx /* and the sp */

	RET_TO_USER

.align 16
composite_call_err:	
	addl $8, %esp /* thread might be useful later, i.e. for termination */		
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
