/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef MMAP_H
#define MMAP_H

#include "cos_types.h"
#include "consts.h"

struct cos_page {
	phys_addr_t addr;
};

void cos_init_memory(void);
void cos_shutdown_memory(void);
static inline unsigned int cos_max_mem_caps(void)
{
	return COS_MAX_MEMORY;
}
phys_addr_t cos_access_page(unsigned long cap_no);
int cos_phys_addr_to_cap(phys_addr_t pa);

#endif
