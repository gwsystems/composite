#ifndef HW_INTS_H
#define HW_INTS_H

void hw_int_init(void);
void hw_int_reset(int tss);
void hw_int_override_sysenter(void *handler);
int  hw_int_override_pagefault(void *handler);
int  hw_int_override_timer(void *handler);
int  hw_int_override_idt(int fault_num, void *handler, int ints_enabled, int dpl);
void hw_int_cos_ipi(void *handler);

#endif
