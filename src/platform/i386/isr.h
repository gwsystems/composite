#ifndef ISR_H
#define ISR_H

#include <chal_asm_inc.h>
#include <thread.h>

enum
{
	IRQ_DIV_BY_ZERO_ERR_FAULT 	= 0,
	IRQ_DEBUG_TRAP 			= 1,
	IRQ_NON_MASKABLE_INTERRUPT 	= 2,
	IRQ_BREAKPOINT_TRAP 		= 3,
	IRQ_OVERFLOW_TRAP 		= 4,
	IRQ_BOUND_RANGE_EXCEED_FAULT 	= 5,
	IRQ_INVALID_OPCODE_FAULT 	= 6,
	IRQ_DEVICE_NOT_AVAIL_FAULT	= 7,
	IRQ_DOUBLE_FAULT_ABORT 		= 8,
	IRQ_INVALID_TSS_FAULT  		= 10,
	IRQ_SEG_NOT_PRESENT_FAULT 	= 11,
	IRQ_STACK_SEG_FAULT 		= 12,
	IRQ_GEN_PROTECT_FAULT 		= 13,
	IRQ_PAGE_FAULT 			= 14,
	IRQ_X87_FLOAT_PT_EXCEPT_FAULT 	= 16,
	IRQ_ALIGN_CHECK_FAULT 		= 17,
	IRQ_MACHINE_CHECK_ABORT 	= 18,
	IRQ_SIMD_FLOAT_PT_EXCEPT_FAULT 	= 19,
	IRQ_VIRTUALIZATION_EXCEPT_FAULT = 20,
	IRQ_SECURITY_EXCEPT_FAULT       = 30,
};

extern void div_by_zero_err_fault_irq(struct regs *);
extern void debug_trap_irq(struct regs *);
extern void breakpoint_trap_irq(struct regs *);
extern void overflow_trap_irq(struct regs *);
extern void bound_range_exceed_fault_irq(struct regs *);
extern void invalid_opcode_fault_irq(struct regs *);
extern void device_not_avail_fault_irq(struct regs *);
extern void double_fault_abort_irq(struct regs *);
extern void invalid_tss_fault_irq(struct regs *);
extern void seg_not_present_fault_irq(struct regs *);
extern void stack_seg_fault_irq(struct regs *);
extern void gen_protect_fault_irq(struct regs *);
extern void page_fault_irq(struct regs *);
extern void x87_float_pt_except_fault_irq(struct regs *);
extern void align_check_fault_irq(struct regs *);
extern void machine_check_abort_irq(struct regs *);
extern void simd_float_pt_except_fault_irq(struct regs *);
extern void virtualization_except_fault_irq(struct regs *);
extern void security_except_fault_irq(struct regs *);

extern void periodic_irq(struct regs *);
extern void handler_hw_33(struct regs *);
extern void handler_hw_34(struct regs *);
extern void handler_hw_35(struct regs *);
extern void serial_irq(struct regs *);
extern void handler_hw_37(struct regs *);
extern void handler_hw_38(struct regs *);
extern void handler_hw_39(struct regs *);
extern void oneshot_irq(struct regs *);
extern void handler_hw_41(struct regs *);
extern void handler_hw_42(struct regs *);
extern void handler_hw_43(struct regs *);
extern void handler_hw_44(struct regs *);
extern void handler_hw_45(struct regs *);
extern void handler_hw_46(struct regs *);
extern void handler_hw_47(struct regs *);
extern void handler_hw_48(struct regs *);
extern void handler_hw_49(struct regs *);
extern void handler_hw_50(struct regs *);
extern void handler_hw_51(struct regs *);
extern void handler_hw_52(struct regs *);
extern void handler_hw_53(struct regs *);
extern void handler_hw_54(struct regs *);
extern void handler_hw_55(struct regs *);
extern void handler_hw_56(struct regs *);
extern void handler_hw_57(struct regs *);
extern void handler_hw_58(struct regs *);
extern void handler_hw_59(struct regs *);
extern void handler_hw_60(struct regs *);
extern void handler_hw_61(struct regs *);
extern void handler_hw_62(struct regs *);
extern void lapic_spurious_irq(struct regs *);
extern void lapic_ipi_asnd_irq(struct regs *);
extern void lapic_timer_irq(struct regs *);

static void
ack_irq(int n)
{
	if (n >= 40) outb(0xA0, 0x20); /* Send reset signal to slave */
	outb(0x20, 0x20);
}

#endif /* ISR_H */
