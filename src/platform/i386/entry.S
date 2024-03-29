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

#define IRQ_ID(ID)		\
.align 4;			\
.global handler_hw_##ID;	\
handler_hw_##ID:		\
	cli; 			\
	pushl $(ID);		\
	jmp irqproc

irqproc:
	IRQPROC(hw_handler)

.text
IRQ(div_by_zero_err_fault)
IRQ(debug_trap)
IRQ(breakpoint_trap)
IRQ(overflow_trap)
IRQ(bound_range_exceed_fault)
IRQ(invalid_opcode_fault)
IRQ(device_not_avail_fault)
IRQ_CODE(double_fault_abort)
IRQ_CODE(invalid_tss_fault)
IRQ_CODE(seg_not_present_fault)
IRQ_CODE(stack_seg_fault)
IRQ_CODE(gen_protect_fault)
IRQ_CODE(page_fault)
IRQ(x87_float_pt_except_fault)
IRQ_CODE(align_check_fault)
IRQ(machine_check_abort)
IRQ(simd_float_pt_except_fault)
IRQ(virtualization_except_fault)
IRQ_CODE(security_except_fault)

IRQ(periodic)
IRQ_ID(33)
IRQ_ID(34)
IRQ_ID(35)
IRQ(serial)
IRQ_ID(37)
IRQ_ID(38)
IRQ_ID(39)
IRQ(oneshot)
IRQ_ID(41)
IRQ_ID(42)
IRQ_ID(43)
IRQ_ID(44)
IRQ_ID(45)
IRQ_ID(46)
IRQ_ID(47)
IRQ_ID(48)
IRQ_ID(49)
IRQ_ID(50)
IRQ_ID(51)
IRQ_ID(52)
IRQ_ID(53)
IRQ_ID(54)
IRQ_ID(55)
IRQ_ID(56)
IRQ_ID(57)
IRQ_ID(58)
IRQ_ID(59)
IRQ_ID(60)
IRQ_ID(61)
IRQ_ID(62)
IRQ(lapic_spurious)
IRQ(lapic_ipi_asnd)
IRQ(lapic_timer)

/*
 * Loads the IDT into the processor
 */
.global idt_flush

idt_flush:
    movl 4(%esp), %eax
    lidt (%eax)
    ret
