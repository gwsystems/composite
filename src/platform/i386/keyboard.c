#include "kernel.h"

#define KEY_DEVICE  0x60
#define KEY_PENDING 0x64

void
keyboard_handler(struct pt_regs *regs)
{
        u16_t scancode = 0;

        lapic_ack();

        while (inb(KEY_PENDING) & 2) {
                /* wait for keypress to be ready */
        }
        scancode = inb(KEY_DEVICE);
        printk("Keyboard press: %d\n", scancode);
}
