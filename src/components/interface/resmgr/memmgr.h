#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_types.h>
#include <cos_kernel_api.h>

/* for simplicity, keep these multiples of PGD_RANGE */
#define MEMMGR_COMP_MAX_HEAP     (1<<25) /* 32MB */

#define MEMMGR_MAX_SHMEM_REGIONS 1024
#define MEMMGR_MAX_SHMEM_SIZE    (1<<22) /* 4MB */
#define MEMMGR_SHMEM_BASE        (0x80000000)

vaddr_t memmgr_heap_page_alloc(spdid_t cur);
vaddr_t memmgr_heap_page_allocn(spdid_t cur, unsigned int num_pages);

int memmgr_shared_page_alloc(spdid_t cur, vaddr_t *pgaddr);
int memmgr_shared_page_allocn(spdid_t cur, int num_pages, vaddr_t *pgaddr);
int memmgr_shared_page_map(spdid_t cur, int id, vaddr_t *pgaddr, int *num_pages);
vaddr_t memmgr_shared_page_vaddr(spdid_t cur, int id);

#endif /* MEMMGR_H */
