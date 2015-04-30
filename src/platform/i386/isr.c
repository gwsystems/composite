#include "shared/cos_types.h"
#include "kernel.h"
#include "io.h"
#include "isr.h"

#define NUM_INTERRUPT_HANDLERS 256

/* If I make these static gcc gets mad because they arn't used localy */
void isr_handler(struct registers);
void irq_handler(struct registers);

static isr_fn_t interrupt_handlers[NUM_INTERRUPT_HANDLERS] = { NULL };

/**
 * ISR handlers
 */
void
isr_handler(struct registers regs)
{
	isr_fn_t handler;
	
	if (regs.int_no > (sizeof(interrupt_handlers) / sizeof(interrupt_handlers[0]))) {
		die("ISR: %d bigger than interrupt handlers array\n", regs.int_no);
	}
	
	handler = interrupt_handlers[regs.int_no];
	if (handler == NULL) {
		die("ISR: %d No handler for event\n", regs.int_no);
	}
	
	handler(&regs);
}

/**
 * IRQ Handler
 */
void
irq_handler(struct registers regs)
{
	isr_fn_t handler;
	
	if (regs.int_no >= 40) {
		outb(0xA0, 0x20); // Send reset signal to slave
	}
	
	// Send reset signal to master
	outb(0x20, 0x20);
	
	if (regs.int_no > (sizeof(interrupt_handlers) / sizeof(interrupt_handlers[0]))) {
		die("IRQ: %d bigger than possible handler\n", regs.int_no);
	}
	
	handler = interrupt_handlers[regs.int_no];
	if (handler == NULL) {
		die("IRQ: %d No handler for event\n", regs.int_no);
	}
	
	handler(&regs);
}

/**
 * Register an interrupt handler
 */
void
register_interrupt_handler(u16_t n, isr_fn_t handler)
{
	if (handler == NULL) {
		die("Attempting to register NULL handler\n");
	}
	
	if (n >= (sizeof(interrupt_handlers) / sizeof(interrupt_handlers[0]))) {
		die("Attempting to register handler bigger than array\n");
	}
	
	interrupt_handlers[n] = handler;
}

