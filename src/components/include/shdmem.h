#ifndef SHDMEM_H
#define SHDMEM_H

#include <cos_kernel_api.h>
#include <consts.h>

int shm_allocate(unsigned int spdid, int num_pages, int shmem_id, int arg4);
int shm_deallocate(int arg1, int arg2, int arg3, int arg4);
int shm_map(int arg1, int arg2, int arg3, int arg4);

/* Each region is the size of a page, this number is arbitrary */
#define SHM_MAX_REGIONS 1000

/*
 * Array of all components that may call down to shdmem api.
 * Since booter and shdmem handler are the same component,
 * have the booter initialize this array during booting process
 */

struct shm_info {
	struct cos_compinfo *cinfo;
	vaddr_t shm_frontier;
	unsigned int my_region_idxs[SHM_MAX_REGIONS];
	unsigned int my_idx;
	/* Boolean value to determine if the PTE for the shdmem range has been allocated */
	int init;
};

struct shm_info shm_infos[MAX_NUM_COMPS];

#endif /* SHDMEM_H */
