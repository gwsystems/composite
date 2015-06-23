#include "chal_asm_inc.h"

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

#define RET_TO_USER \
	sti; 	    \
	sysexit

/* The entire IPC path should be in __ipc_entry section. */
.section __ipc_entry, "ax"
.align 16
.globl sysenter_entry
sysenter_entry:
	SAVE_REGS_GENERAL

	pushl %esp /* pt_reg */
	call composite_sysenter_handler
	addl $4, %esp

	testl %eax, %eax
	jne restore_from_thd

	RESTORE_REGS_GENERAL
	RET_TO_USER

/* TODO: autogenerate these with offsetof within gen_asm_offsets.c */
#define THD_REGS 8
PT_EBX		= 0x00
PT_ECX		= 0x04
PT_EDX		= 0x08
PT_ESI		= 0x0C
PT_EDI		= 0x10
PT_EBP		= 0x14
PT_EAX		= 0x18
PT_DS		= 0x1C
PT_ES		= 0x20
PT_FS           = 0x24
PT_GS           = 0x28
PT_ORIG_EAX	= 0x2C
PT_EIP		= 0x30
PT_CS		= 0x34
PT_EFLAGS	= 0x38
PT_OLDESP	= 0x3C
PT_OLDSS	= 0x40

/* we are changing thread contexts, reload all registers */
.align 16
restore_from_thd:
	pushl %ecx
	pushl %edx
	call cos_get_curr_thd_asm
	popl %edx
	popl %ecx

	addl $THD_REGS, %eax /* offsetof(struct thread, regs) */
	/* restore from preemption */
	movl PT_EBX(%eax), %ebx
	movl PT_ECX(%eax), %ecx
	movl PT_EDX(%eax), %edx
	movl PT_ESI(%eax), %esi
	movl PT_EDI(%eax), %edi
	movl PT_EBP(%eax), %ebp

	/* set up the activation record required by iret */
	pushl $(SEL_UDSEG)
	pushl PT_OLDESP(%eax)
	pushl PT_EFLAGS(%eax)
	pushl $(SEL_UCSEG)
	pushl PT_EIP(%eax)

	movl PT_EAX(%eax), %eax

	iret
