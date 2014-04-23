#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

typedef unsigned char      u8_t;
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;
typedef signed char      s8_t;
typedef signed short int s16_t;
typedef signed int       s32_t;
typedef signed long long s64_t;

typedef unsigned long vaddr_t;
typedef unsigned long paddr_t;

void *chal_va2pa(void *va) { return va; }
void *chal_pa2va(void *pa) { return pa; }

#define HALF_CACHE_ALIGNED __attribute__((aligned(32)))
#define CACHE_ALIGNED __attribute__((aligned(64)))
#define PAGE_ALIGNED __attribute__((aligned(4096)))
#include <captbl.h>
#include <pgtbl.h>
#include <cap_ops.h>
//#include <liveness_tbl.h>
#include <thread.h>
//#include <component.h>
//#include <inv.h>

u8_t boot_comp_captbl[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pgd[PAGE_SIZE]    PAGE_ALIGNED;
u8_t boot_comp_pte_vm[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pte_pm[PAGE_SIZE] PAGE_ALIGNED;

unsigned long sys_maxmem      = 1<<10; /* 4M of physical memory (2^10 pages) */
unsigned long sys_llbooter_sz = 10;    /* how many pages is the llbooter? */

void
kern_boot_comp(void)
{
	struct captbl *ct;
	pgtbl_t pt;
	unsigned int i;

	ct = captbl_create(boot_comp_captbl);
	assert(ct);
	pt = pgtbl_create(boot_comp_pgd);
	pgtbl_init_pte(boot_comp_pte_vm);
	pgtbl_init_pte(boot_comp_pte_pm);
	/* Virtual memory at 4MB */
	if (pgtbl_intern_expand(pt, (1<<22), boot_comp_pte_vm, PGTBL_INTERN_DEF)) assert(0);
	assert(sys_llbooter_sz <= 1<<10); /* need another pte otherwise */
	for (i = 0 ; i < sys_llbooter_sz ; i++) {
		if (pgtbl_mapping_add(pt, ((1<<22) + PAGE_SIZE * i), 0xADEAD000, 
				      PGTBL_PRESENT | PGTBL_USER | PGTBL_WRITABLE)) assert(0);
	}
	/* Physical memory starting at 1GB */
	if (pgtbl_intern_expand(pt, (1<<30), boot_comp_pte_pm, PGTBL_INTERN_DEF)) assert(0);
	assert(sys_maxmem <= 1<<10); /* need another pte otherwise */
	for (i = 0 ; i < sys_maxmem ; i++) {
 		if (pgtbl_mapping_add(pt, ((1<<30) + (PAGE_SIZE * i)), 
				      (i * PAGE_SIZE), PGTBL_COSFRAME)) assert(0);
	}
}

void 
kern_main(void)
{
	cap_init();
	ltbl_init();
	comp_init();
	thd_init();
//	inv_init();

	kern_boot_comp();
}

int main(void) { kern_main(); return 0; }
