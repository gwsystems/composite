#include <chal.h>
#include <shared/cos_types.h>
#include "kernel.h"

u32_t free_thd_id = 1;
char timer_detector[PAGE_SIZE] PAGE_ALIGNED;
extern void *cos_kmem, *cos_kmem_base;

void *
chal_pa2va(void *address)
{
        return address;
}

void *
chal_va2pa(void *address)
{
        return address;
}

void *chal_alloc_kern_mem(int order)
{
        cos_kmem_base = cos_kmem = (void*)KERNEL_BASE_PHYSICAL_ADDRESS;
	return (void*)cos_kmem;
}

void chal_free_kern_mem(void *mem, int order)
{
}

int chal_attempt_arcv(struct cap_arcv *arcv)
{
	return 0;
}

void chal_send_ipi(int cpuid)
{
}
