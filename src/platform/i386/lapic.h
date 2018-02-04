#ifndef LAPIC_H
#define LAPIC_H

#include "apic_cntl.h"

void         lapic_iter(struct lapic_cntl *);
u32_t        lapic_find_localaddr(void *l);
void         lapic_set_page(u32_t page);
void         lapic_timer_init(void);
void         lapic_set_timer(int timer_type, cycles_t deadline);
u32_t        lapic_get_ccr(void);
void         lapic_timer_calibration(u32_t ratio);
extern u32_t lapic_timer_calib_init;

void smp_init(void);

#endif /* LAPIC_H */
