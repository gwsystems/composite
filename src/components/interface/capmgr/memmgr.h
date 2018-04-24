#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cos_component.h>

vaddr_t memmgr_va2pa(vaddr_t vaddr);
vaddr_t memmgr_pa2va_map(paddr_t pa, unsigned int len);

vaddr_t memmgr_heap_page_alloc(void);
vaddr_t memmgr_heap_page_allocn(unsigned long num_pages);

cbuf_t        memmgr_shared_page_alloc(vaddr_t *pgaddr);
cbuf_t        memmgr_shared_page_allocn(unsigned long num_pages, vaddr_t *pgaddr);
unsigned long memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr);

/* This magic number is double the tls size defined in RK */
#define TLS_AREA_SIZE 32
#define TLS_NUM_PAGES (round_up_to_page(TLS_AREA_SIZE * MAX_NUM_THREADS) / PAGE_SIZE)
#define TLS_BASE_ADDR 0x70000000

void *memmgr_tls_alloc(unsigned int dst_tid);
void *_memmgr_tls_alloc_and_set(void *area);

static void *
memmgr_tls_alloc_and_set(void *area)
{
	void *addr = _memmgr_tls_alloc_and_set(area);

	/* Set area within addr for this tid */
	*(void **)addr = area;

	return addr;
}

#endif /* MEMMGR_H */
