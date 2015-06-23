#ifndef KERNEL_H
#define KERNEL_H

#include "shared/cos_config.h"
#include "shared/cos_types.h"
#include "chal.h"
#include "multiboot.h"

#include "chal_asm_inc.h"

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
void paging_init(void);

//void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
