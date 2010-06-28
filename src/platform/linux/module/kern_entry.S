/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <asm/asm-offsets.h>
#include <asm/segment.h>
#include <asm/thread_info.h>
#include <linux/linkage.h>
#include "asym_exec_dom.h"
#include "../../../kernel/include/asm_ipc_defs.h"
	
/* We cannot use sysexit when switching from executive to child as
then the executive would not be able to modify the ecx and edx
registers of the child because they are stored on the child's
stack.  By default we also do the same when going from child to
executive, but this is not strictly necessary, so define the
next value if you want to use sysexit instead of iret to trap
back to the executive.*/
#define SYSEXIT
	
/* from entry.S */	
EBX		= 0x00
ECX		= 0x04
EDX		= 0x08
ESI		= 0x0C
EDI		= 0x10
EBP		= 0x14
EAX		= 0x18
DS		= 0x1C
ES		= 0x20
FS              = 0x24
GS              = 0x28
ORIG_EAX	= 0x2C
EIP		= 0x30
CS		= 0x34
EFLAGS		= 0x38
OLDESP		= 0x3C
OLDSS		= 0x40

CF_MASK		= 0x00000001
TF_MASK		= 0x00000100
IF_MASK		= 0x00000200
DF_MASK		= 0x00000400 
NT_MASK		= 0x00004000
VM_MASK		= 0x00020000
		
#define SAVE_ALL \
	cld; \
	pushl %gs; \
	pushl %fs; \
	pushl %es; \
	pushl %ds; \
	pushl %eax; \
	pushl %ebp; \
	pushl %edi; \
	pushl %esi; \
	pushl %edx; \
	pushl %ecx; \
	pushl %ebx; \
	movl $(__USER_DS), %edx; \
	movl %edx, %ds; \
	movl %edx, %es; \
	movl $(__KERNEL_PERCPU), %edx; \
	movl %edx, %fs;	\
	movl $(__KERNEL_STACK_CANARY), %edx; \
	movl %edx, %gs

#define RESTORE_INT_REGS \
	popl %ebx;	\
	popl %ecx;	\
	popl %edx;	\
	popl %esi;	\
	popl %edi;	\
	popl %ebp;	\
	popl %eax

#define RESTORE_REGS	\
	RESTORE_INT_REGS; \
	popl %ds;	\
	popl %es;	\
	popl %fs;       \
	popl %gs
/* 
 * This is all information that is needed within this file, and
 * external to this file.  It is here, so that it is easier to
 * concurrently use hijack modules and composite modules.
 */
.data
.align 16
.globl cos_sysenter_addr
cos_sysenter_addr:	
	.long 0
.globl cos_temp_esp_storage
cos_temp_esp_storage:	
	.long 0
	
.text
	
	ALIGN
asym_exec_domain_child_executive:
	popl %ebp
	popfl
	sti
	pushl $(__USER_DS)
	pushl %ebp
	pushfl
	pushl $(__USER_CS)
	pushl $0xffffe410 /* FIXME: hardcode in SYSENTER_RETURN */
	pushl %eax
	SAVE_ALL

	call module_switch_to_executive

#ifdef SYSEXIT
	cli
	movl $0xffffe410, %edx
	movl EIP(%esp), %edx
	movl OLDESP(%esp), %ecx
	sti
	sysexit
#else /* !SYSEXIT */
	cli
	RESTORE_REGS
	addl $4, %esp
	iret 
#endif /* SYSEXIT */

	ALIGN

asym_exec_domain_executive_child:
	popl %ebp
	popfl
	sti
	pushl $(__USER_DS)
	pushl %ebp
	pushfl
	pushl $(__USER_CS)
	pushl $0xffffe410 /* FIXME: hardcode in SYSENTER_RETURN */
	pushl %eax
	SAVE_ALL
	
	pushl %ebx /* user ptr arg to child context */
	call module_switch_to_child
	add  $0x4, %esp
		
#ifdef SYSEXIT
	cli
	movl $0xffffe410, %edx
	movl EIP(%esp), %edx
	movl OLDESP(%esp), %ecx
	sti
	sysexit
#else /* !SYSEXIT */
	cli
	RESTORE_REGS
	addl $4, %esp
	iret 
#endif
	/* use iret instead of sysexit because ecx and edx would have
	to be restored to the user-level stack where they were saved
	before which is more complicated. */
	
	ALIGN
/* this is above syscall entry for branch predict */
hijack_checks:
#ifdef COMPOSITE
	/* do composite ipc path */
#endif
	cmpl $HIJACK_SWITCH_TO_GUEST_ID, %eax 
	je asym_exec_domain_executive_child /* executive invoking child */
	jmp normal_syscall

	/* 
	 * The following code is never called (it is dead code, as
	 * the last line is an unconditional jmp) but I'm keeping it as
	 * an example for normal syscall interposition even for the
	 * executive (when normal syscall semantics don't apply 
	 */
	 
	cmpl $90, %eax /* mmap must be done with the correct mm */
	jne normal_syscall
	cmpl $192, %eax /* same with mmap2 */
	jne normal_syscall

	/* if we have a syscall that will possibly change the memory 
	   maps, we need to possibly change mms */
	SAVE_ALL
	pushl %esp /* pt_regs */
	/*	call mem_mapping_syscall*/
	RESTORE_REGS
	jmp normal_syscall

ALIGN
ENTRY(asym_exec_dom_entry)
	movl %esp, cos_temp_esp_storage /* This is so we can restore it to continue normal linux syscall paths if necessary */
	/* FIXME: not smp safe */
	movl TSS_sysenter_sp0(%esp),%esp

	/*
	 * Composite invocations are indicated by the contents of %eax:
	 * +-------------+----+--...
	 * |   cap_inv   | sc | normal OS (linux) syscalls
	 * +-------------+----+--...
	 * 32            |    COS_SYSCALL_OFFSET
	 *               COS_CAPABILITY_OFFSET
 	 *	
	 * Where each character represents a bit.
	 * cap_inv:	The capability to invoke
	 * sc:	 Make a composite OS system-call
	 */
	cmpl $((1<<COS_CAPABILITY_OFFSET)-1), %eax /* 2^20-1 shift for capability invocations */
	ja composite_call_ipc /* component invocation */
	je composite_ret_ipc /* component return */
	cmpl $((1<<COS_SYSCALL_OFFSET)-1), %eax /* composite syscall */
	ja composite_make_syscall

	pushfl /* we are about to clobber the accumulator */
	pushl %ebp /* we need some storage space */
	GET_THREAD_INFO(%ebp)
	testw $(_TIF_VIRTUAL_SYSCALL), TI_flags(%ebp) /* are we in a child of an exec domain? */
	jnz asym_exec_domain_child_executive

	testw $(_TIF_HIJACK_ENV), TI_flags(%ebp)
	jnz hijack_checks
	/* fall through */

        /* FIXME: truly pointless, remove this chunk */
	SAVE_ALL
	pushl %esp
/*	call syscall_interposition */
	addl $4, %esp
	RESTORE_REGS
	
normal_syscall:
		
	popl %ebp
	popfl
	movl cos_temp_esp_storage, %esp
	jmp  *(cos_sysenter_addr) /* sti will happen in syscall path */

	/* never get to here on syscall...*/


#ifdef NIL
.align 64
ENTRY(page_fault_interposition)
	jmp *(cos_default_page_fault_handler)
	/*
	 * we don't want our function to muck with the previously 
	 * active registers, so save them...  Note that the stack 
	 * already contains cs, ds, flags, sp, eip, and the error code.   
	 */
	SAVE_ALL
	movl ORIG_EAX(%esp), %edx
	movl %esp, %eax /* pt_regs argument */
	call main_page_fault_interposition
	test %eax, %eax
	jnz normal_pf
	RESTORE_REGS
	addl $4, %esp 	/* status/error code */
	iret

	/* restore the saved regs before we call the linux handler */
normal_pf:
	/*
	RESTORE_REGS
	I have no idea why it's happening, but occasionally when we
	try and access memory in the page fault here, we get a GPF.  I
	believe this is because the segment registers get corrupted.
	So instead of reloading (the faulty) previous segement regs,
	just restore the integer registers and fs here (not ds/cs)
	instead.
	*/
	RESTORE_INT_REGS
	addl $8, %esp
	popl %fs
	popl %gs
	jmp *(cos_default_page_fault_handler)
#endif
	
/*
 * Trap will cause this handler to trigger.  The stack
 * already contains cs, ds, flags, sp, eip, and 0.  We will save
 * registers, call a composite handler, decide if we can deal with it
 * there, or if we should call the linux handler, and if so,
 * return the machine state (stack etc) to how it was when the
 * trap occurred, and call the linux handler.
 */
#define COS_CREATE_TRAP_NO_ERRCODE(name)	 		\
.data;								\
.globl cos_default_##name##_handler; 				\
cos_default_##name##_handler:					\
.long 0;							\
.text;								\
.align 16;					 		\
ENTRY(name##_interposition)					\
	pushl $0;						\
	SAVE_ALL; 						\
	movl %esp, %eax; 					\
	movl ORIG_EAX(%eax), %edx;				\
	call main_##name##_interposition;			\
	test %eax, %eax;					\
	jnz 1f;							\
	RESTORE_REGS;						\
	addl $4, %esp;						\
	iret;							\
1:	/* linux path */					\
	RESTORE_INT_REGS;					\
	addl $8, %esp;						\
	popl %fs;						\
	popl %gs;						\
	addl $4, %esp;						\
	jmp *(cos_default_##name##_handler)	

#define TEST
#ifdef TEST
#define COS_CREATE_TRAP_ERRCODE(name)		 		\
.data;								\
.globl cos_default_##name##_handler; 				\
cos_default_##name##_handler:					\
.long 0;							\
.text;								\
.align 16;					 		\
ENTRY(name##_interposition)					\
	jmp 0xc049a978

#else	/* TEST */
	
/* Same as above, but this time, there is an errorcode provided by the HW */
#define COS_CREATE_TRAP_ERRCODE(name)		 		\
.data;								\
.globl cos_default_##name##_handler; 				\
cos_default_##name##_handler:					\
.long 0;							\
.text;								\
.align 16;					 		\
ENTRY(name##_interposition)					\
	SAVE_ALL; 						\
	movl %esp, %eax; 					\
	movl ORIG_EAX(%eax), %edx;				\
	jmp 1f			 ;\
	call main_##name##_interposition;			\
	test %eax, %eax;					\
	jnz 1f;							\
	RESTORE_REGS;						\
	addl $4, %esp;						\
	iret;							\
1:	/* linux path */					\
	RESTORE_INT_REGS;					\
	addl $8, %esp;						\
	popl %fs;						\
	popl %gs;						\
	jmp *(cos_default_##name##_handler)	
#endif 	/* TEST */
	
COS_CREATE_TRAP_NO_ERRCODE(div_fault)
COS_CREATE_TRAP_ERRCODE(page_fault)
