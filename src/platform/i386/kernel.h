#ifndef KERNEL_H
#define KERNEL_H

#include <shared/cos_config.h>
#include <shared/cos_types.h>
#include "chal.h"
#include "multiboot.h"

#include "chal_asm_inc.h"
#include <thd.h>
#include <hw.h>

#include "vga.h"
#include "serial.h"
#include "hpet.h"
#include "miniacpi.h"
#include "lapic.h"
#include "pic.h"

void  tss_init(void);
void  idt_init(void);
void  gdt_init(void);
void  user_init(void);
void  paging_init(void);
void  kern_paging_map_init(void *pa);

void tls_update(u32_t addr);

int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
