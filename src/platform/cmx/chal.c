#include <chal.h>
#include <shared/cos_types.h>
#include "kernel.h"

u32_t free_thd_id = 1;
char timer_detector[PAGE_SIZE] PAGE_ALIGNED;
extern void *cos_kmem, *cos_kmem_base;

paddr_t chal_kernel_mem_pa;

void *
chal_pa2va(paddr_t address)
{ return (void*)(address+COS_MEM_KERN_START_VA); }

paddr_t
chal_va2pa(void *address)
{ return (paddr_t)(address-COS_MEM_KERN_START_VA); }

void *
chal_alloc_kern_mem(int order)
{ return 0;/*mem_kmem_start();*/ }

void chal_free_kern_mem(void *mem, int order) {}

int
chal_attempt_arcv(struct cap_arcv *arcv)
{ return 0; }

void chal_send_ipi(int cpuid) {}

void
chal_khalt(void)
{ khalt(); }

void
chal_init(void)
{ chal_kernel_mem_pa = 0;/*chal_va2pa(mem_kmem_start());*/ }
