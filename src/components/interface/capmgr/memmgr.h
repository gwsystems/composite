#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_types.h>

vaddr_t memmgr_va2pa(vaddr_t vaddr);
vaddr_t memmgr_pa2va_map(paddr_t pa, unsigned int len);

vaddr_t memmgr_heap_page_alloc(void);
vaddr_t memmgr_heap_page_allocn(unsigned int num_pages);

int memmgr_shared_page_alloc(vaddr_t *pgaddr);
int memmgr_shared_page_allocn(int num_pages, vaddr_t *pgaddr);
int memmgr_shared_page_map(int id, vaddr_t *pgaddr);

#endif /* MEMMGR_H */
