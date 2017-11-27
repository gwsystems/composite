#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <shdmem.h>

/*
 * For generic shared memory, the client component is responsible for allocating memory
 * from this shared memory manager.
 * The server component should then map in memory upon the request of the client component
 */

static vaddr_t shm_master_regions[SHM_MAX_REGIONS];
/* Until we have to implement the deallocate, just increment this */
static unsigned int shm_master_idx = 0;

/* cos_compinfo for the booter component when using vkernel_init.c for the booter */
struct cos_compinfo *shm_cinfo;

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
		shm_infos[spdid].my_regions[i] = -1;
	}

	shm_infos[spdid].init = 1;
}

static void
__print_region_idxs(unsigned int spdid)
{
	unsigned int count = 0;

	printc("\tPrinting regions for the shm_info, spdid: %d\n", spdid);
	while (count < SHM_MAX_REGIONS) {
		if (shm_infos[spdid].my_regions[count]) {
			printc("\t\tidx: %u, master region: %d\n", count, (int)shm_infos[spdid].my_regions[count]);
		}
		count++;
	}
}

vaddr_t
shm_get_vaddr(unsigned int spdid, unsigned int id)
{
	printc("IN SHM_GET_VADDR;\n");
	assert(id < SHM_MAX_REGIONS && shm_infos[spdid].cinfo && shm_infos[spdid].shm_frontier);

	return shm_infos[spdid].my_regions[id];
}

int
shm_allocate(unsigned int spdid, unsigned int num_pages)
{
	printc("IN SHM_ALLOCATE\n");
	vaddr_t src_pg, dst_pg, unused;
	struct shm_info *comp_shm_info;
	int ret, id;

	/* FIXME, this function is a critial section, syncronize this sh*t */

	assert(shm_cinfo && \
		((int)spdid > -1) && \
		shm_infos[spdid].cinfo && \
		shm_infos[spdid].shm_frontier && \
		num_pages);

	if (num_pages > 1) {
		/* Unimplemented TODO */
		printc("num_pages > 1, unimplemented, TODO\n");
		return -1;
	}

	/* Initialize the shm_info for this spdid if it has not been initialized yet */
	if (!shm_infos[spdid].init) { __shm_infos_init(spdid); }

	comp_shm_info = &shm_infos[spdid];
	assert(comp_shm_info);

	src_pg = (vaddr_t)cos_page_bump_alloc(shm_cinfo);
	assert(src_pg);
	/* Source Page comes from component managing shared memory, this is the page we keep in shm_master_regions*/
	assert(shm_master_idx < SHM_MAX_REGIONS);
	shm_master_regions[shm_master_idx] = src_pg;

	/* Get address to map into */
	id = shm_master_idx;
	dst_pg = comp_shm_info->shm_frontier;
	assert(dst_pg);
        comp_shm_info->my_regions[id] = dst_pg;

	shm_master_idx++;

	ret = cos_mem_alias_at(comp_shm_info->cinfo, comp_shm_info->shm_frontier, shm_cinfo, src_pg);
	assert(dst_pg && !ret);
	comp_shm_info->shm_frontier += PAGE_SIZE;

	return id;
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

	printc("Hello from shm_deallocate - UNIMPLEMENTED\n");
	assert(0);

	return 0;
}

int
shm_map(unsigned int spdid, unsigned int id)
{
	printc("IN SHM_MAP\n");
	vaddr_t src_pg, dst_pg;
	int ret;
	struct shm_info *comp_shm_info;

	assert(id < SHM_MAX_REGIONS && \
		shm_infos[spdid].shm_frontier && \
		shm_infos[spdid].cinfo);

	comp_shm_info = &shm_infos[spdid];

	/* Initialize the shm_info for this spdid if it has not been initialized yet */
	if (!comp_shm_info->init) { __shm_infos_init(spdid); }

	src_pg = shm_master_regions[id];
	dst_pg = comp_shm_info->shm_frontier;
	comp_shm_info->my_regions[id] = dst_pg;

	ret = cos_mem_alias_at(comp_shm_info->cinfo, comp_shm_info->shm_frontier, shm_cinfo, src_pg);
	assert(dst_pg && !ret);
	comp_shm_info->shm_frontier += PAGE_SIZE;

	return id;
}

void
cos_init(void)
{
	struct cos_defcompinfo *dci;
	int ret;

	printc("Welcome to the shdmem component\n");
	printc("Getting cos_compinfo for ourselves...");
	dci = cos_defcompinfo_curr_get();
	assert(dci);
	shm_cinfo = cos_compinfo_get(dci);
	assert(shm_cinfo);
	printc(" done\n");

	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
}
