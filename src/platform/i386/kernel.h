#ifndef KERNEL_H
#define KERNEL_H

#include "shared/cos_types.h"
#include "chal.h"

/* Segment selectors for the GDT */
#define SEL_NULL	0x00
#define SEL_KDSEG       0x10    /* Kernel data selector. */
#define SEL_KCSEG       0x08    /* Kernel code selector. */
#define SEL_UCSEG       0x1B    /* User code selector. */
#define SEL_UDSEG       0x23    /* User data selector. */
#define SEL_TSS         0x28    /* Task-state segment. */
#define SEL_CNT         6       /* Number of segments. */

#define KERNEL_BASE_PHYSICAL_ADDRESS	0x00100000

/* A not so nice way of oopsing */
#define die(fmt, ...) do {              \
    printk(fmt,##__VA_ARGS__);   \
    khalt();				\
} while(0)

enum log_level {
    RAW,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

#ifdef ENABLE_CONSOLE
void vga_clear(void);
void vga_puts(const char *s);
void console_init(void);
#endif

#ifdef ENABLE_SERIAL
void serial_init(void);
#endif

#ifdef ENABLE_TIMER
void timer_init(u32_t frequency);
#endif

void tss_init(void);
void idt_init(void);
void gdt_init(void);
void user_init(void);
void paging_init(u32_t nmods, u32_t *mods);

//void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
