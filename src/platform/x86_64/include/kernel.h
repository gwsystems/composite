#pragma once

#include <cos_types.h>
#include <chal.h>
#include <multiboot2.h>
#include <state.h>
#include <thread.h>

void serial_init(void);

#define PRINTK(format, ...) printk("%d: " format, coreid(), ## __VA_ARGS__)

/* These numbers map directly to actual timers in the HPET */
typedef enum {
	TIMER_PERIODIC = 0,
	TIMER_ONESHOT  = 1,
} timer_type_t;

#define TIMER_DEFAULT_US_INTERARRIVAL 1000 /* US = microseconds */

void *device_pa2va(paddr_t dev_addr);
void *device_map_mem(paddr_t dev_addr, unsigned int pt_extra_flags);

void  timer_set(timer_type_t timer_type, u64_t cycles);
void  timer_init(void);
u64_t timer_find_hpet(void *timer);
void  timer_thd_init(struct thread *t);
void *timer_initialize_hpet(void *timer);

void  tss_init(const coreid_t cpu_id);
void  idt_init(const coreid_t cpu_id);
void  gdt_init(const coreid_t cpu_id);
void  user_init(void);
void  paging_init(void);

void  acpi_init(void);
void *acpi_find_rsdt(void);
void *acpi_find_timer(void);
void  acpi_set_rsdt_page(u32_t);
void  kern_paging_map_init(void *pa);

void *acpi_find_apic(void);
void  acpi_shutdown(void);

int   lapic_find_localaddr(void *l);
void  lapic_timer_init(void);
void  lapic_init(void);
void  lapic_set_timer(int timer_type, cos_time_t deadline);
u32_t lapic_get_ccr(void);
void  lapic_timer_calibration(u32_t ratio);
int   lapic_timer_calibrated(void);
void  lapic_asnd_ipi_send(const coreid_t cpu_id);

void smp_init(volatile int *cores_ready);

void tls_update(u32_t addr);

// void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));
void print_regs(struct regs *r);

void khalt(void) __attribute__((noreturn));

/*
 * Symbols from the linker script, references the end of kernel image's
 * memory. This should be beyond the retypeable pages, and the
 * remaining memory might be statically user-level, VM typed.
 */
extern u8_t kernel_start_va, kernel_end_va;
