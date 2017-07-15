/*
 * TODO
 * See Trello for master list
 * Add new sinv allocs for these functions in vkernel_init.c
 */

#include <cos_kernel_api.h>
#include <shdmem.h>

/* FIXME change the types of these parameters */
vaddr_t
shm_allocate(unsigned int spdid, int num_pages, int shmem_id, int arg4)
{
	vaddr_t src_pg, dst_pg, unused, ret_vaddr = 0;
	int ret;
	static int done = 0;
	/* cos_compinfo for the booter component when using vkernel_init.c for the booter */
	extern struct cos_compinfo *vk_cinfo;

	printc("\tspdid of calling component: %d\n", spdid);
	printc("\tcos_compinfo of calling component: %p\n", shm_infos[spdid].cinfo);
	printc("\tshdmem frontier of calling component: %p\n", shm_infos[spdid].shm_frontier);
	printc("\tbooter's cinfo: %p\n", vk_cinfo);
	printc("\t# of desired pages: %d\n", num_pages);

	assert(vk_cinfo && ((int)spdid > -1) && shm_infos[spdid].cinfo &&  shm_infos[spdid].shm_frontier && num_pages);

	src_pg = (vaddr_t)cos_page_bump_alloc(vk_cinfo);
	assert(src_pg);
	printc("src_pg: %p\n", src_pg);

	/* FIXME, Think carefully about this, this can be a problem with a multithreaded application */
	dst_pg = shm_infos[spdid].shm_frontier;
	printc("dst_pg: %p\n", dst_pg);

	/*
	 * FIXME
	 * This is a hack to deal with creating a page table entry for the kernel component.
	 * Only doing this once upon the first page request.
	 * To fix, check how much memory each PTE covers and have the shmem api track for the
	 * components when they must allocate another pte
	 */

	if (!done) {
		ret_vaddr = cos_pgtbl_intern_alloc(shm_infos[spdid].cinfo, shm_infos[spdid].cinfo->pgtbl_cap,
			dst_pg, PAGE_SIZE);
		done++;
	} else {
		ret_vaddr = dst_pg;
	}

	assert(ret_vaddr == dst_pg);
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
	 * TODO
	 * Similar functionality to cbuf_c_register
	 *
	 * arg1 - spdid_t spdid of the new component to map in
	 * arg2 - long cbid of the page that was already alloced
	 *
	 */
	printc("Hello from shm_map\n");
	return 0;
}
