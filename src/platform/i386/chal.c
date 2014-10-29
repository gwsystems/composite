#include <chal.h>
#include <shared/cos_types.h>

u32_t free_thd_id = 1;
char timer_detector[PAGE_SIZE] PAGE_ALIGNED;

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
	return (void*)0;
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
