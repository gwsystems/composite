#ifndef KERNEL_H
#define KERNEL_H

#include <shared/cos_config.h>
#include <shared/cos_types.h>
#include <shared/util.h>
#include "chal.h"
#include "multiboot.h"

#include "chal_asm_inc.h"
#include <thd.h>
#include <hw.h>

#ifdef ENABLE_CONSOLE
void vga_clear(void);
void vga_puts(const char *s);
void console_init(void);
#endif

#ifdef ENABLE_VGA
void vga_high_init(void);
void vga_init(void);
void vga_puts(const char *str);
#endif

#ifdef ENABLE_SERIAL
void serial_init(void);
#endif

/* These numbers map directly to actual timers in the HPET */
typedef enum {
	TIMER_PERIODIC = 0,
	TIMER_ONESHOT  = 1,
} timer_type_t;

#define TIMER_DEFAULT_US_INTERARRIVAL 1000 /* US = microseconds */

void  timer_set(timer_type_t timer_type, u64_t cycles);
void  timer_init(void);
u64_t timer_find_hpet(void *timer);
void  timer_set_hpet_page(u32_t page);
void  timer_thd_init(struct thread *t);

void  tss_init(const cpuid_t cpu_id);
void  idt_init(const cpuid_t cpu_id);
void  gdt_init(const cpuid_t cpu_id);
void  user_init(void);
void  paging_init(void);
void *acpi_find_rsdt(void);
void *acpi_find_timer(void);
void  acpi_set_rsdt_page(u32_t);
void  kern_paging_map_init(void *pa);

void *       acpi_find_apic(void);
u32_t        lapic_find_localaddr(void *l);
void         lapic_set_page(u32_t page);
void         lapic_timer_init(void);
void         lapic_init(void);
void         lapic_set_timer(int timer_type, cycles_t deadline);
u32_t        lapic_get_ccr(void);
void         lapic_timer_calibration(u32_t ratio);
void         lapic_asnd_ipi_send(const cpuid_t cpu_id);
extern volatile u32_t lapic_timer_calib_init;

void smp_init(volatile int *cores_ready);

void tls_update(u32_t addr);

// void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
