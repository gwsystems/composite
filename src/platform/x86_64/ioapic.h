#ifndef IOAPIC_H
#define IOAPIC_H

#include "ioapic_cntl.h"

// void ioapic_init(void);

// void ioapic_iter(struct ioapic_cntl *);
// // void ioapic_int_mask(int irq);
// // void ioapic_int_unmask(int irq);

// void ioapic_int_disable(int irq);
// void ioapic_int_enable(int irq, cpuid_t cpu_id);

// void ioapic_int_override(struct intsrcovrride_cntl *);


//Origianlly pic.h

// void pic_init(void);
// void pic_enable(void);
// void pic_disable(void);

// static void
// pic_ack_irq(int n)
// {
// 	if (n >= 40) outb(0xA0, 0x20); /* Send reset signal to slave */
// 	outb(0x20, 0x20);
// }

// //originally io.h
// /**
//  * Write byte to specific port
//  */
// static inline void
// outb(u16_t port, u8_t value)
// {
// 	__asm__ __volatile__("outb %1, %0" : : "dN"(port), "a"(value));
// }

// /**
//  * Read byte from port
//  */
// static inline u8_t
// inb(u16_t port)
// {
// 	u8_t ret;

// 	__asm__ __volatile__("inb %1, %0" : "=a"(ret) : "dN"(port));

// 	return ret;
// }


#endif /* IOAPIC_H */
