#ifndef ISR_H
#define ISR_H

#include "shared/cos_types.h"
#include "io.h"
#include "chal_asm_inc.h"

enum {
	IRQ_DOUBLE_FAULT = 8,
	IRQ_PAGE_FAULT   = 14,
	IRQ_PIT          = 32,
	IRQ_KEYBOARD     = 33,
	IRQ_SERIAL       = 36
};
extern void double_fault_irq(struct pt_regs *);
extern void page_fault_irq(struct pt_regs *);
extern void timer_irq(struct pt_regs *);
extern void keyboard_irq(struct pt_regs *);
extern void serial_irq(struct pt_regs *);

static void
ack_irq(int n)
{
	if (n >= 40) outb(0xA0, 0x20); /* Send reset signal to slave */
	outb(0x20, 0x20);
}

#endif /* ISR_H */
