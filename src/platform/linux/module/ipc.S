/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 * Modified by: Qi Wang, interwq@gwu.edu, 2014
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
	ERROR_OUT

#define SAVE_REGS_GENERAL 	\
	subl $40, %esp; 	\
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

#define KERN_PRINT_ENABLED

#ifdef KERN_PRINT_ENABLED
/* set up seg reg, which is needed for printk */
#define SEG_REG_SETUP 			\
	pushl %edx;			\
	movl $(__KERNEL_PERCPU), %edx;	\
	movl %edx, %fs;			\
	popl %edx
#else
#define SEG_REG_SETUP 
#endif

/* Composite sysenter entry. */
.globl composite_sysenter_entry
.align 32
composite_sysenter_entry:
	pushl %edx
	movl $(__KERNEL_PERCPU), %edx
	movl %edx, %fs
	popl %edx

//	SEG_REG_SETUP
	
	/* Use pt_regs as input */
	SAVE_REGS_GENERAL

	pushl %esp /* pt_reg */
	call composite_sysenter_dispatcher
	addl $4, %esp

	testl %eax, %eax
	jne ret_from_preemption

	RESTORE_REGS_GENERAL
	RET_TO_USER


.align 4
ret_from_preemption:
	LOAD_CURR_THD_EAX;
	
	addl $THD_REGS, %eax /* offsetof(struct thread, regs) */
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
