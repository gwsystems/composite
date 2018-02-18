#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_types.h>
#include <cos_kernel_api.h>

/* for simplicity, keep these multiples of PGD_RANGE */
#define MEMMGR_COMP_MAX_HEAP     (1<<25) /* 32MB */
#define MEMMGR_COMP_MAX_SHARED   (1<<22) /* 4MB */
#define MEMMGR_COMP_MAX_SHREGION (MEMMGR_COMP_MAX_SHARED / PAGE_SIZE)
#define MEMMGR_COMP_SHARED_BASE  (0x80000000)

vaddr_t memmgr_heap_page_alloc(spdid_t cur);
vaddr_t memmgr_heap_page_allocn(spdid_t cur, unsigned int num);

int memmgr_shared_page_alloc(spdid_t cur, vaddr_t *pgaddr);
int memmgr_shared_page_allocn(spdid_t cur, int num, vaddr_t *pgaddr);
int memmgr_shared_page_map_range(spdid_t cur, spdid_t src, int src_idx, int off, int num_pages, vaddr_t *pgaddr);
int memmgr_shared_page_map(spdid_t cur, spdid_t src, int src_idx, vaddr_t *pgaddr);

vaddr_t memmgr_shared_page_vaddr(spdid_t cur, int cur_idx);

#endif /* MEMMGR_H */
