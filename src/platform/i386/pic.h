#ifndef PIC_H
#define PIC_H

#include "io.h"

void pic_init(void);

static void
pic_ack_irq(int n)
{
	if (n >= 40) outb(0xA0, 0x20); /* Send reset signal to slave */
	outb(0x20, 0x20);
}

#endif /* PIC_H */
