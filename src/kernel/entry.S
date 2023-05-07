#include <cos_arch_regs.h>

/* The entire IPC path should be in __ipc_entry section. */
.section .ipc_fastpath, "ax"
.align 16
.globl syscall_entry
syscall_entry:
	swapgs           /* activate the kernel gs */
	movq %gs:0, %rsp /* from it get the stack pointer */
	PUSH_REGS_SYSCALL
	pushq %rsp       /* pointer to the registers to use a retval */
	mov %rsp, %rdi   /* pointer to pointer to struct reg argument */
	call syscall_handler
	popq %rdi        /* retrieve the registers we're to switch to */
	movq %rdi, %rsp  /* stack pointer now points to the top of the register structure */
	testq %rax, %rax /* syscall_hander returned `0`? -> full register restore */
	jne restore_all_registers /* for the syscall, this jumps forward, hopefully statically predicated to not taken */
	POP_REGS_RET_SYSCALL
/* we are changing thread contexts, so reload all registers */
.align 16
restore_all_registers:
	POP_REGS_RET_TRAP
/* We're out of the fast-path now, so use the normal code segment */
.text

#define IRQPROC(fn)  			\
	PUSH_REGS_TRAP; 		\
	mov %rsp, %rdi; 		\
	call fn;			\
	testq %rax, %rax;		\
	jne restore_all_registers;	\
	POP_REGS_RET_SYSCALL;		\

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
	push $0;  		\
	IRQPROC(fn##_handler);	\

#define IRQ_CODE(fn)		\
.align 16;			\
.global fn##_irq;		\
fn##_irq: 			\
	cli; 			\
	IRQPROC(fn##_handler);	\

#define IRQ_ID(ID)		\
.align 16;			\
.global handler_hw_##ID;	\
handler_hw_##ID:		\
	cli; 			\
	push $(ID);		\
	jmp irqproc;		\

irqproc:
	IRQPROC(hw_handler)

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
IRQ(smid_float_pt_except_fault)
IRQ(virtualization_except_fault)
IRQ_CODE(security_except_fault)

IRQ(periodic)
IRQ(keyboard)
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
