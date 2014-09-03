/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef MMAP_H
#define MMAP_H

#include "shared/cos_types.h"
#include "shared/consts.h"

struct cos_page {
	/* paddr_t addr; */
	//need ref_cnt for each frame here.
};

int cos_init_memory(void);
void cos_shutdown_memory(void);
static inline unsigned int cos_max_mem_caps(void)
{
	return COS_MAX_MEMORY;
}
paddr_t cos_access_page(unsigned long cap_no);
int cos_paddr_to_cap(paddr_t pa);

paddr_t cos_access_kernel_page(unsigned long cap_no);
int cos_kernel_paddr_to_cap(paddr_t pa);

#endif
