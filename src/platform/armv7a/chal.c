#include <chal.h>
#include <shared/cos_types.h>
#include "kernel.h"
#include "mem_layout.h"
#include "chal_cpu.h"
#include "irq.h"

u32_t        free_thd_id = 1;
char         timer_detector[PAGE_SIZE] PAGE_ALIGNED;
extern void *cos_kmem, *cos_kmem_base;
u32_t        chal_msr_mhz = 0;

paddr_t chal_kernel_mem_pa;

void *
chal_pa2va(paddr_t address)
{
	return (void *)(address + COS_MEM_KERN_START_VA);
}

paddr_t
chal_va2pa(void *address)
{
	return (paddr_t)(address - COS_MEM_KERN_START_VA);
}

void *
chal_alloc_kern_mem(int order)
{
	return mem_kmem_start();
}

void
chal_free_kern_mem(void *mem, int order)
{
}

int
chal_attempt_arcv(struct cap_arcv *arcv)
{
	return 0;
}

void
chal_send_ipi(int cpuid)
{
}

void
chal_khalt(void)
{
	khalt();
}

void
chal_init(void)
{
	/* Initialize timers, etc */
	__cos_cav7_int_init();
	timer_init();
	l2cache_init();
	/* Initialize the vector table */
	printk("CAV7-Vector table: 0x%x\r\n", (unsigned long)&__cos_cav7_vector_table);
	__cos_cav7_vbar_set((unsigned long)&__cos_cav7_vector_table);

	printk("CAV7-Init: complete\r\n");
}

void
chal_tls_update(vaddr_t tlsaddr)
{
}

int
chal_cyc_usec(void)
{
	return CYC_PER_USEC;
}

void
_exit(int code)
{
	/* dead loop */
	while (1)
		;
}

int
_kill(int pid, int sig)
{
	return -1;
}

int
_getpid(void)
{
	return -1;
}

char *
_sbrk(int incr)
{
	return (char *)(0);
}
