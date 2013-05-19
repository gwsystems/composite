#ifndef _ISR_H_
#define _ISR_H_

#include "types.h"

struct registers {
   uintptr_t ds;
   uintptr_t edi;
   uintptr_t esi;
   uintptr_t ebp;
   uintptr_t esp;
   uintptr_t ebx;
   uintptr_t edx;
   uintptr_t ecx;
   uintptr_t eax;
   uintptr_t int_no;
   uintptr_t err_code;
   uintptr_t eip;
   uintptr_t cs;
   uintptr_t eflags;
   uintptr_t useresp;
   uintptr_t ss;
};

enum IRQS {
    IRQ0    = 32,
    IRQ1    = 33,
    IRQ2    = 34,
    IRQ3    = 35,
    IRQ4    = 36,
    IRQ5    = 37,
    IRQ6    = 38,
    IRQ7    = 39,
    IRQ8    = 40,
    IRQ9    = 41,
    IRQ10   = 42,
    IRQ11   = 43,
    IRQ12   = 44,
    IRQ13   = 45,
    IRQ14   = 46,
    IRQ15   = 47,
};

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

typedef void (*isr_t)(struct registers *);

void register_interrupt_handler(uint16_t n, isr_t handler);

#endif
