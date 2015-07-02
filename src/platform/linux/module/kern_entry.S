/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Modified by: Qi Wang, interwq@gwu.edu, 2014
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <asm/asm-offsets.h>
#include <asm/segment.h>
#include <asm/thread_info.h>
#include <asm/irqflags.h>	
#include <linux/linkage.h>
#include "asym_exec_dom.h"
#include "../../../kernel/include/asm_ipc_defs.h"
#include "../../../kernel/include/shared/cos_config.h"
#include "cos_irq_vectors.h"

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

#define SAVE_ALL    \
	cld;        \
	pushl $0 ;  \
	pushl %fs;  \
	pushl %es;  \
	pushl %ds;  \
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
	movl %edx, %fs
	
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
	addl $4, %esp

/* Composite pt_regs save/restore */
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

/* push a known value onto the stack, so that we can tell, if we get
   an interrupt after the sti, that we are in a composite IPC
   call. See comment in hijack.c:get_user_regs.  Essentially, if we 
   get an interrupt, this makes the stack look like if user-level
   were preempted (in terms of register contents) */
#define RET_TO_USER \
	pushl $0; \
	pushl $0; \
	sti; 	  \
	sysexit

/* Loads current thread structure to eax register. ECD and EDX can be
 * clobbered, as per the calling convention. */
#define LOAD_CURR_THD_EAX \
	pushl %ecx		   ;\
	pushl %edx		   ;\
	call cos_get_curr_thd_asm  ;\
	popl %edx		   ;\
	popl %ecx

/* The entire IPC path should be in __ipc_entry section. */
.section __ipc_entry, "ax"
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
.globl sysenter_interposition_entry
.align 4096
sysenter_interposition_entry:
	cmpl $(1<<COS_SYSCALL_OFFSET), %eax
	jb linux_syscall
	/* Composite entry */
	/* Use pt_regs as input */
	SAVE_REGS_GENERAL

	pushl %esp /* pt_reg */
	call composite_syscall_handler
	addl $4, %esp

	testl %eax, %eax
	jne ret_from_preemption

	RESTORE_REGS_GENERAL
	RET_TO_USER

#define THD_REGS 8 /* offsetof(struct thread, regs) */

.align 16
ret_from_preemption:
	LOAD_CURR_THD_EAX

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

	/* __ipc_entry section done. */	
.text

.align 16
linux_syscall:
	/* Restore TSS, then get correct sp0. */
	/* First get TSS from cos_cpu_local_info */
	andl $(LINUX_INFO_PAGE_MASK), %esp
	/* Space reserved for Linux thread info struct. */
	addl $(LINUX_THREAD_INFO_RESERVE), %esp
	movl (%esp), %esp
	/* getting sp0 from TSS */
	movl TSS_sysenter_sp0(%esp),%esp

	SAVE_ALL
	pushl %eax
	call hijack_syscall_monitor
	addl $4, %esp
	RESTORE_REGS

	/* We reserve the position in the stack to get the TSS. */
	/* see the comments of get_TSS in hijack.c */	
	pushl $0
	SAVE_ALL
	movl %esp, %eax 	
	call get_TSS
	RESTORE_REGS
	popl %esp
	jmp  *(cos_default_sysenter_addr) /* sti will happen in syscall path */
	/* never get to here on syscall...*/

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
.globl name##_interposition;	 				\
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
	RESTORE_REGS;						\
	addl $4, %esp;						\
	jmp *(cos_default_##name##_handler)	
	
/* Same as above, but this time, there is an errorcode provided by the HW */
#define COS_CREATE_TRAP_ERRCODE(name)		 		\
.data;								\
.globl cos_default_##name##_handler; 				\
cos_default_##name##_handler:					\
.long 0;							\
.text;								\
.globl name##_interposition;	 				\
.align 16;					 		\
ENTRY(name##_interposition)					\
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
	RESTORE_REGS;						\
	jmp *(cos_default_##name##_handler)	


/* Push the irq# onto stack as the second parameter. Note that in Linux
 * interrupt handling rountine, macro BUILD_INTERRUPT pushes $~(irq). */
#define COS_CREATE_HANDLER_NO_INTERPOSITION(name, irq)		\
.data;								\
.long 0;							\
.text;								\
.globl name##_handler;		 				\
.align 16;					 		\
ENTRY(name##_handler)						\
	pushl $(irq);						\
	SAVE_ALL; 						\
	movl %esp, %eax; 					\
	movl ORIG_EAX(%eax), %edx;				\
	call main_##name##_handler;				\
	RESTORE_REGS;						\
	addl $4, %esp;						\
	iret;
	
COS_CREATE_TRAP_NO_ERRCODE(div_fault)
COS_CREATE_TRAP_ERRCODE(page_fault)
COS_CREATE_TRAP_NO_ERRCODE(reg_save)
COS_CREATE_TRAP_NO_ERRCODE(timer)
#ifdef FPU_ENABLED
COS_CREATE_TRAP_ERRCODE(fpu_not_available)
#endif
COS_CREATE_HANDLER_NO_INTERPOSITION(ipi, COS_IPI_VECTOR)

/* The page fault trampoline for kernel faults.  See explanation in relocate_page_fault_handler:hw_ints.c */
.data
.align 4096
.globl cos_page_fault_page_tramp
cos_page_fault_page_tramp:
.globl cos_default_page_fault_handler_tramp
cos_default_page_fault_handler_tramp:
.long 0
.globl cos_interpose_page_fault_handler_tramp
cos_interpose_page_fault_handler_tramp:
.long 0
.globl page_fault_interposition_tramp
.align 16
page_fault_interposition_tramp:
	testl $4,(%esp)  /* *sp = error code, 0x4 = user (=1) or kern (=0) */
	jz 1f
	jmp *(cos_interpose_page_fault_handler_tramp)
.globl cos_post_interpose_deref_addr_tramp
cos_post_interpose_deref_addr_tramp: /* for the previous address */
1:	
	jmp *(cos_default_page_fault_handler_tramp)
.globl cos_post_default_deref_addr_tramp
cos_post_default_deref_addr_tramp:   /* for the previous address */
	.align 4096
