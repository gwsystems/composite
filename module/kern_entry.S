#include <asm/asm-offsets.h>
#include <asm/segment.h>
#include <asm/thread_info.h>
#include <linux/linkage.h>
#include "asym_exec_dom.h"

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
ORIG_EAX	= 0x24
EIP		= 0x28
CS		= 0x2C
EFLAGS		= 0x30
OLDESP		= 0x34
OLDSS		= 0x38

CF_MASK		= 0x00000001
TF_MASK		= 0x00000100
IF_MASK		= 0x00000200
DF_MASK		= 0x00000400 
NT_MASK		= 0x00004000
VM_MASK		= 0x00020000
		
#define SAVE_ALL \
	cld; \
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
	movl %edx, %es;

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
1:	popl %ds;	\
2:	popl %es;	\


/* 
 * This is all information that is needed within this file, and
 * external to this file.  It is here, so that it is easier to
 * concurrently use hijack modules and composite modules.
 */
.data
.globl default_page_fault_handler
default_page_fault_handler:	
	.long 0
.globl sysenter_addr
sysenter_addr:	
	.long 0
.globl temp_esp_storage
temp_esp_storage:	
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
	movl %esp, temp_esp_storage /* This is so we can restore it to continue normal linux syscall paths if necessary */
	/* FIXME: not smp safe */
	movl TSS_sysenter_esp0(%esp),%esp

	/*
	 * Composite invocations are indicated by the contents of %eax:
	 * +-+-----------+----+--...
	 * |S| cap_inv   | sc | normal OS (linux) syscalls
	 * +-+-----------+----+--...
	 * 32|           20   16
	 *   31
	 *
	 * Where each character represents a bit.
	 * S:	Register save flag on a component invocation
	 * cap_inv:	The capability to invoke
	 * sc:	 Make a composite OS system-call
	 */
	cmpl $((1<<20)-1), %eax /* 2^16-1 shift for capability invocations */
	ja composite_call_ipc /* component invocation */
	je composite_ret_ipc /* component return */
	cmpl $((1<<16)-1), %eax /* composite syscall */
	ja composite_make_syscall

	pushfl /* we are about to clobber the accumulator */
	pushl %ebp /* we need some storage space */
	GET_THREAD_INFO(%ebp)
	testw $(_TIF_VIRTUAL_SYSCALL), TI_flags(%ebp) /* are we in a child of an exec domain? */
	jnz asym_exec_domain_child_executive

	testw $(_TIF_HIJACK_ENV), TI_flags(%ebp)
	jnz hijack_checks
	/* fall through */
	
/* branch optimize for normal system calls */
normal_syscall:
	popl %ebp
	popfl
	movl temp_esp_storage, %esp
	jmp  *(sysenter_addr) /* sti will happen in syscall path */

	/* never get to here on syscall...*/
	
	ALIGN
ENTRY(page_fault_interposition)
	/*
	 * we don't want our function to muck with the previously 
	 * active registers, so save them...  Note that the stack 
	 * already contains cs, ds, flags, sp and eip.  This 
	 * format matches SAVE_ALL and struct pt_regs.
	 */
	pushl %es
	pushl %ds
	pushl %eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	/*	GET_THREAD_INFO(%ebp)*/
#ifdef COMPOSITE
	testw $(_TIF_COMPOSITE_ENV), TI_flags(%ebp)
	jz not_composite_pf
        /* enter composite pf call */
	RESTORE_REGS
	addl $4, %esp
	iret /* work??? */
#endif
not_composite_pf:	
/*	testw $(_TIF_HIJACK_ENV), TI_flags(%ebp)
	jz normal_pf
*/
	call main_page_fault_interposition
	test %eax, %eax
	jne normal_pf
	RESTORE_REGS
	iret

	/* restore the saved regs before we call the linux handler */
normal_pf:	
	RESTORE_REGS
	jmp *(default_page_fault_handler)

