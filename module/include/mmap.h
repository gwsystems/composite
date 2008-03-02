#ifndef MMAP_H
#define MMAP_H

#include "cos_types.h"

#define COS_MAX_MEMORY 2048

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

#endif
