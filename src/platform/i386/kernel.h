#ifndef KERNEL_H
#define KERNEL_H

#include "shared/cos_config.h"
#include "shared/cos_types.h"
#include "chal.h"
#include "multiboot.h"

#include "chal_asm_inc.h"
#include <thd.h>

/* A not so nice way of oopsing */
#define die(fmt, ...) do {              \
    printk(fmt,##__VA_ARGS__);   \
    khalt();				\
} while(0)

#ifdef ENABLE_CONSOLE
void vga_clear(void);
void vga_puts(const char *s);
void console_init(void);
#endif

#ifdef ENABLE_SERIAL
void serial_init(void);
#endif

typedef enum {
    TIMER_FREQUENCY,
    TIMER_ONESHOT
} timer_type_t;

#define DEFAULT_FREQUENCY 100000000

void timer_set(timer_type_t timer_type, u64_t cycles);
void timer_init(timer_type_t timer_type, u64_t cycles);
u64_t timer_find_hpet(void *timer);
void timer_set_hpet_page(u32_t page);
void timer_thd_init(struct thread *t);

void tss_init(void);
void idt_init(void);
void gdt_init(void);
void user_init(void);
void paging_init(void);
void *acpi_find_rsdt(void);
void *acpi_find_timer(void);
void acpi_set_rsdt_page(u32_t);
void kern_paging_map_init(void *pa);

//void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
