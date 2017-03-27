#ifndef LINUX_PGTBL_H
#define LINUX_PGTBL_H

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/pgtable.h>
#include "../../../kernel/include/chal.h"

void pgtbl_print_valid_entries(paddr_t pt);

#endif	/* LINUX_PGTBL_H */
