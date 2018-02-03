#include "pic.h"

#define PIC_IRQ_BASE    0x20
#define PIC_ALL_DISABLE 0xFF
#define PIC_ALL_ENABLE  0x00

/* Information taken from: http://wiki.osdev.org/PIC */
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_CMD PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_CMD PIC2
#define PIC2_DATA (PIC2 + 1)

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8 and 70, as configured by default */
#define PIC_ICW1_ICW4      0x01 /* ICW4 (not) needed */
#define PIC_ICW1_SINGLE    0x02 /* Single (cascade) mode */
#define PIC_ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define PIC_ICW1_LEVEL     0x08 /* Level triggered (edge) mode */
#define PIC_ICW1_INIT      0x10 /* Initialization - required! */

#define PIC_ICW4_8086       0x01 /* 8086/88 (MCS-80/85) mode */
#define PIC_ICW4_AUTO       0x02 /* Auto (normal) EOI */
#define PIC_ICW4_BUF_SLAVE  0x08 /* Buffered mode/slave */
#define PIC_ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define PIC_ICW4_SFNM       0x10 /* Special fully nested (not) */
#define PIC_ICW1_ICW4       0x01

static void
pic_disable(void)
{
	outb(PIC1_DATA, PIC_ALL_DISABLE);
	outb(PIC2_DATA, PIC_ALL_DISABLE);
}

static void
pic_enable(void)
{
	outb(PIC1_DATA, PIC_ALL_ENABLE);
	outb(PIC2_DATA, PIC_ALL_ENABLE);
}

void
pic_init(void)
{
	outb(PIC1_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
	outb(PIC2_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
	outb(PIC1_DATA, PIC_IRQ_BASE);
	outb(PIC2_DATA, PIC_IRQ_BASE + 8);
	outb(PIC1_DATA, 4);
	outb(PIC2_DATA, 2);
	outb(PIC1_DATA, PIC_ICW4_8086);
	outb(PIC2_DATA, PIC_ICW4_8086);

	pic_enable();
}
