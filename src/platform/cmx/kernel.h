#ifndef KERNEL_H
#define KERNEL_H

#include "chal/shared/cos_config.h"
#include "chal/shared/cos_types.h"
#include "chal.h"

#include "chal_asm_inc.h"
#include <thd.h>
#include <hw.h>

void user_init(void);
void paging_init(void);
void kern_paging_map_init(void *pa);

void tls_update(u32_t addr);

void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
