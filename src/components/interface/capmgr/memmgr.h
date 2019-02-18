#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_types.h>
#include <cos_component.h>

vaddr_t memmgr_heap_page_alloc(void);
vaddr_t memmgr_heap_page_allocn(unsigned long num_pages);

cbuf_t        memmgr_shared_page_alloc(vaddr_t *pgaddr);
cbuf_t        memmgr_shared_page_allocn(unsigned long num_pages, vaddr_t *pgaddr);
unsigned long memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr);

vaddr_t memmgr_initdcbpage_retrieve(void);
vaddr_t memmgr_dcbpage_allocn(unsigned long num_pages);

#endif /* MEMMGR_H */
