#include <cos_kernel_api.h>
#include <shdmem.h>

static vaddr_t shm_master_regions[SHM_MAX_REGIONS];
/* Until we have to implement the deallocate, just increment this */
static unsigned int shm_master_idx = 0;

static void
__shm_infos_init(unsigned int spdid)
{
	int i;

	/* Allocate second level PTE for the shdmem regions */
	/* FIXME, should we be passing in PAGE_SIZE? */
	cos_pgtbl_intern_alloc(shm_infos[spdid].cinfo, shm_infos[spdid].cinfo->pgtbl_cap,
		shm_infos[spdid].shm_frontier,
		PAGE_SIZE);

	/* Set all region idxs to -1 */
	for (i = 0 ; i < SHM_MAX_REGIONS ; i++ ) {
		shm_infos[spdid].my_region_idxs[i] = -1;
	}

	shm_infos[spdid].init = 1;
}

static void
__print_region_idxs(unsigned int spdid)
{
	unsigned int count = 0;

	printc("\tPrinting regions for the shm_info, spdid: %d\n", spdid);
	while (count < shm_infos[spdid].my_idx) {
		printc("\t\tidx: %d, master region: %d\n", count, shm_infos[spdid].my_region_idxs[count]);
		count++;
	}
}

int
shm_allocate(unsigned int spdid, int num_pages, int shmem_id, int arg4)
{
	vaddr_t src_pg, dst_pg, unused;
	int ret, idx;
	/* cos_compinfo for the booter component when using vkernel_init.c for the booter */
	extern struct cos_compinfo *vk_cinfo;

	/* FIXME, this function is a critial section, syncronize this sh*t */

	assert(vk_cinfo && \
		((int)spdid > -1) && \
		shm_infos[spdid].cinfo && \
		shm_infos[spdid].shm_frontier && \
		num_pages);

	/* Initialize the shm_info for this spdid if it has not been initialized yet */
	if (!shm_infos[spdid].init) { __shm_infos_init(spdid); }

	src_pg = (vaddr_t)cos_page_bump_alloc(vk_cinfo);
	assert(src_pg);
	/* Source Page comes from component managing shared memory, this is the page we keep in shm_master_regions*/
	assert(shm_master_idx < SHM_MAX_REGIONS);
	shm_master_regions[shm_master_idx] = src_pg;

	/* Get address to map into */
	dst_pg = shm_infos[spdid].shm_frontier;
	idx = shm_infos[spdid].my_idx;
	assert(idx < SHM_MAX_REGIONS);
	/* Keep track of the regions we have been provided */
	shm_infos[spdid].my_region_idxs[idx] = shm_master_idx;

	shm_master_idx++;
	shm_infos[spdid].my_idx++;

	__print_region_idxs(spdid);

	ret = cos_mem_alias_at(shm_infos[spdid].cinfo, shm_infos[spdid].shm_frontier, vk_cinfo, src_pg);
	assert(dst_pg && !ret);
	shm_infos[spdid].shm_frontier += PAGE_SIZE;

	printc("\n");

	printc("\tRunning TEST\n");
	printc("\tWriting a byte to page in booter...\n");
	*((char *)src_pg) = 'a';
	printc("\tDone, 'a' was written, 'a' is %d as an int\n", (int)'a');

	printc("\n");

	return dst_pg;
}

int
shm_deallocate(int arg1, int arg2, int arg3, int arg4)
{
	/*
	 * TODO
	 * This function holds functionality for both cbuf_c_delete
	 * and cbuf_c_retrieve.
	 * Only support delete for now, the goal for this application
	 * shared memory is that it is set up once and left, when we
	 * are done with it, we will just be looking to delete it
	 *
	 * Delete will get rid of the whole shared memory and return it
	 * Takes spdid_t spdid, int cbid
	 */

	printc("Hello from shm_deallocate\n");
	return 0;
}

int
shm_map(int arg1, int arg2, int arg3, int arg4)
{
	/*
	 * arg1 - spdid_t spdid of the new component to map in
	 * arg2 - long cbid of the page that was already alloced
	 */
	return 0;
}
