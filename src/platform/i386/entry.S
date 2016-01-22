#include "chal_asm_inc.h"

/* Composite pt_regs save/restore */
#define SAVE_REGS_GENERAL 	\
	pushl %eax;		\
	pushl %ebp;		\
	pushl %edi;		\
	pushl %esi;		\
	pushl %edx;		\
	pushl %ecx;		\
	pushl %ebx

#define SAVE_REGS_ALL 		\
	subl $40, %esp; 	\
	SAVE_REGS_GENERAL

/* When return to user: ecx => sp, edx => ip. */
#define RESTORE_REGS_GENERAL	\
	popl %ebx;		\
	popl %ecx;		\
	popl %edx;		\
	popl %esi;		\
	popl %edi;		\
	popl %ebp;		\
	popl %eax

#define RESTORE_REGS_ALL	\
	RESTORE_REGS_GENERAL; 	\
	addl $40, %esp

#define RET_TO_USER \
	sti; 	    \
	sysexit

/* The entire IPC path should be in __ipc_entry section. */
.section __ipc_entry, "ax"
.align 16
.globl sysenter_entry
sysenter_entry:
	SAVE_REGS_ALL

	pushl %esp /* pt_reg */
	call composite_syscall_handler
	addl $4, %esp

	testl %eax, %eax
	jne restore_from_thd

	RESTORE_REGS_ALL
	RET_TO_USER

/* we are changing thread contexts, reload all registers */
.align 16
restore_from_thd:
	RESTORE_REGS_GENERAL
	popl %ds
	popl %es
	popl %fs
	popl %gs

	addl $4, %esp

	sti
	iret

#define IRQPROC(fn)  \
	pushl $(SEL_UGSEG); 	\
	pushl $(SEL_UDSEG); 	\
	pushl $(SEL_UDSEG);	\
	pushl $(SEL_UDSEG);	\
	SAVE_REGS_GENERAL; 	\
	movl $(SEL_KDSEG), %eax;\
	movl %eax, %ds;		\
	pushl %esp;		\
	call fn;		\
	addl $4, %esp;		\
	testl %eax, %eax;	\
	jne restore_from_thd;	\
	RESTORE_REGS_ALL;	\
	RET_TO_USER

/*
 * Convention:
 * C handler is IRQHANDLER_handler, and the irq assembly
 * routine is IRQHANDLER_irq.
 */
#define IRQ(fn)		 	\
.align 16;			\
.global fn##_irq;		\
fn##_irq: 			\
	cli; 			\
	pushl $0;  		\
	IRQPROC(fn##_handler)

#define IRQ_CODE(fn)		\
.align 16;			\
.global fn##_irq;		\
fn##_irq: 			\
	cli; 			\
	IRQPROC(fn##_handler)

.text
IRQ_CODE(double_fault)
IRQ_CODE(page_fault)
/* IRQ(timer) */
IRQ(keyboard)
IRQ(serial)
IRQ(periodic)
IRQ(oneshot)

/*
 * Loads the IDT into the processor
 */
.global idt_flush

idt_flush:
    movl 4(%esp), %eax
    lidt (%eax)
    ret
