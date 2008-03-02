#include "include/mmap.h"

static struct cos_page cos_pages[COS_MAX_MEMORY];

extern void *cos_alloc_page(void);
extern void *cos_free_page(void *page);
extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);

void cos_init_memory(void) 
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		cos_pages[i].addr = 0;
	}

	return;
}

void cos_shutdown_memory(void)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		phys_addr_t addr = cos_pages[i].addr;

		if (addr != 0) {
			cos_free_page(pa_to_va((void*)addr));
			addr = 0;
		}
	}
}

phys_addr_t cos_access_page(unsigned long cap_no)
{
	phys_addr_t addr;

	if (cap_no > COS_MAX_MEMORY) return 0;

	addr = cos_pages[cap_no].addr;
	if (addr == 0) {
		void *r = cos_alloc_page();

		if (NULL == r) {
			printk("cos: could not allocate page for cos memory\n");
			return 0;
		}
		addr = cos_pages[cap_no].addr = (phys_addr_t)va_to_pa(r);
	}

	return addr;
}
