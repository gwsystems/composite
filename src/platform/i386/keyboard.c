#include "kernel.h"

#define KEY_DEVICE  0x60
#define KEY_PENDING 0x64

int
keyboard_handler(struct pt_regs *regs)
{
        u16_t scancode = 0;
	int preempt = 1;
	static int chg = 0;

        lapic_ack();

        while (inb(KEY_PENDING) & 2) {
                /* wait for keypress to be ready */
        }
        scancode = inb(KEY_DEVICE);
        PRINTK("Keyboard press: %d\n", scancode);

	chal_irq_enable(HW_KEYBOARD, chg);
	chg = !chg;

	return preempt;
}
