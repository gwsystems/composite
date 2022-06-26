/*
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>

vaddr_t       memmgr_heap_page_alloc(void);
vaddr_t       memmgr_virt_to_phys(vaddr_t vaddr);
vaddr_t       memmgr_map_phys_to_virt(paddr_t paddr, size_t size);

vaddr_t       memmgr_heap_page_allocn(unsigned long num_pages);
vaddr_t       COS_STUB_DECL(memmgr_heap_page_allocn)(unsigned long num_pages);

vaddr_t       memmgr_heap_page_allocn_aligned(unsigned long num_pages, unsigned long align);
vaddr_t       COS_STUB_DECL(memmgr_heap_page_allocn_aligned)(unsigned long num_pages, unsigned long align);

cbuf_t        memmgr_shared_page_alloc(vaddr_t *pgaddr);

cbuf_t        memmgr_shared_page_allocn(unsigned long num_pages, vaddr_t *pgaddr);
cbuf_t        COS_STUB_DECL(memmgr_shared_page_allocn)(unsigned long num_pages, vaddr_t *pgaddr);

cbuf_t        memmgr_shared_page_allocn_aligned(unsigned long num_pages, unsigned long align, vaddr_t *pgaddr);
cbuf_t        COS_STUB_DECL(memmgr_shared_page_allocn_aligned)(unsigned long num_pages, unsigned long align, vaddr_t *pgaddr);

unsigned long memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr);
unsigned long COS_STUB_DECL(memmgr_shared_page_map)(cbuf_t id, vaddr_t *pgaddr);

unsigned long memmgr_shared_page_map_aligned(cbuf_t id, unsigned long align, vaddr_t *pgaddr);
unsigned long COS_STUB_DECL(memmgr_shared_page_map_aligned)(cbuf_t id, unsigned long align, vaddr_t *pgaddr);


#endif /* MEMMGR_H */
