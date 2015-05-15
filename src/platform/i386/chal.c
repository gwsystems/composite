#include <chal.h>
#include <shared/cos_types.h>
#include "kernel.h"

u32_t free_thd_id = 1;
char timer_detector[PAGE_SIZE] PAGE_ALIGNED;
extern void *cos_kmem, *cos_kmem_base;

void *
chal_pa2va(paddr_t address)
{
        return (void*)(address+COS_MEM_KERN_START_VA);
}

paddr_t
chal_va2pa(void *address)
{
        return (paddr_t)(address-COS_MEM_KERN_START_VA);
}

void *chal_alloc_kern_mem(int order)
{
        cos_kmem_base = cos_kmem = (void*)COS_MEM_KERN_PA;
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
