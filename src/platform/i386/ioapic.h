#ifndef IOAPIC_H
#define IOAPIC_H

#include "apic_cntl.h"

void ioapic_init(void);

void ioapic_iter(struct ioapic_cntl *);
// void ioapic_int_mask(int irq);
// void ioapic_int_unmask(int irq);

void ioapic_int_disable(int irq);
void ioapic_int_enable(int irq, cpuid_t cpu_id);

void ioapic_int_override(struct intsrcovrride_cntl *);

#endif /* IOAPIC_H */
