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

#include "vga.h"
#include "serial.h"
#include "hpet.h"
#include "acpi.h"
#include "lapic.h"
#include "pic.h"
#include "ioapic.h"

int vm_map_superpage(u32_t addr, int nocache);
void kern_paging_map_init(void *);
void paging_init(void);
void tss_init(cpuid_t);
void gdt_init(cpuid_t);
void idt_init(cpuid_t);

void tls_update(u32_t addr);

int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
