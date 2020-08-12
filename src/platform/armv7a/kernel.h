#ifndef KERNEL_H
#define KERNEL_H

#include <shared/cos_config.h>
#include <shared/cos_types.h>
#include <chal/shared/util.h>
#include "chal.h"
#include "multiboot.h"

#include "chal_asm_inc.h"
#include <thd.h>
#include <hw.h>

#ifdef ENABLE_SERIAL
void serial_init(void);
#endif

/* These numbers map directly to actual timers in the HPET */
typedef enum
{
	TIMER_PERIODIC = 0,
	TIMER_ONESHOT  = 1,
} timer_type_t;

#define CYC_PER_USEC 767 /* Set to 767 MHz */

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

void serial_send(char out);

void smp_init(volatile int *cores_ready);

void tls_update(u32_t addr);

// void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
