#ifndef ISR_H
#define ISR_H

#include "shared/cos_types.h"

struct registers {
   u32_t ds;
   u32_t edi;
   u32_t esi;
   u32_t ebp;
   u32_t esp;
   u32_t ebx;
   u32_t edx;
   u32_t ecx;
   u32_t eax;
   u32_t int_no;
   u32_t err_code;
   u32_t eip;
   u32_t cs;
   u32_t eflags;
   u32_t useresp;
   u32_t ss;
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

typedef void (*isr_fn_t)(struct registers *);

void register_interrupt_handler(u16_t n, isr_fn_t handler);

#endif /* ISR_H */
