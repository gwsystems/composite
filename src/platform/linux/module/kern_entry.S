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

#define ASM_PAGE_MASK (~(4096-1))
	
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

/* 
 * This is all information that is needed within this file, and
 * external to this file.  It is here, so that it is easier to
 * concurrently use hijack modules and composite modules.
 */
.data
.align 16
//.globl cos_sysenter_addr
//cos_sysenter_addr:	
//	.long 0
//.globl cos_temp_esp_storage
//cos_temp_esp_storage:	
//	.long 0

.text
ALIGN
ENTRY(sysenter_interposition_entry)
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

	cmpl $(COS_ASYNC_CAP_FLAG), %eax /* Non-static cap? */
	ja composite_async_call /* Async invocation */

	cmpl $((1<<COS_CAPABILITY_OFFSET)-1), %eax /* 2^20-1 shift for capability invocations */
	ja composite_call_ipc /* component invocation */
	je composite_ret_ipc  /* component return */

	cmpl $((1<<COS_SYSCALL_OFFSET)-1), %eax /* composite syscall */
	ja cos_syscall_thunk

	SAVE_ALL
	pushl %eax
	call hijack_syscall_monitor
	addl $4, %esp
	RESTORE_REGS
normal_syscall:
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

/* cos system calls might call linux functions, so we need fs */
cos_syscall_thunk:
	pushl %edx
	movl $(__KERNEL_PERCPU), %edx
	movl %edx, %fs
	popl %edx
	jmp composite_make_syscall

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
	
COS_CREATE_TRAP_NO_ERRCODE(div_fault)
COS_CREATE_TRAP_ERRCODE(page_fault)
COS_CREATE_TRAP_NO_ERRCODE(reg_save)

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
