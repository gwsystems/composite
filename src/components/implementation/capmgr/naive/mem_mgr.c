#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <memmgr.h>
#include "cap_info.h"

vaddr_t
memmgr_heap_page_allocn_intern(spdid_t cur, unsigned int npages)
{
	struct cos_compinfo  *cap_ci  = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cap_comp_info *cur_rci = cap_info_comp_find(cur);
	struct cos_compinfo  *cur_ci  = cap_info_ci(cur_rci);
	vaddr_t src_pg, dst_pg;
	unsigned int off = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci)) return 0;
	if (!cur_ci) return 0;

	src_pg = (vaddr_t)cos_page_bump_allocn(cap_ci, npages * PAGE_SIZE);
	if (!src_pg) return 0;
	dst_pg = cos_mem_aliasn(cur_ci, cap_ci, src_pg, npages * PAGE_SIZE);

	return dst_pg;
}

int
memmgr_shared_page_allocn_intern(vaddr_t *pgaddr, int *unused, spdid_t cur, int npages)
{
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cap_shmem_info *cur_shi = cap_info_shmem_info(cur_rci);
	int shmidx = -1;

	if (!cur_rci || !cap_info_init_check(cur_rci)) goto done;
	if (!cur_shi) goto done;

	shmidx = cap_shmem_region_alloc(cur_shi, npages);
	if (shmidx < 0) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, shmidx);

done:
	return shmidx;
}

int
memmgr_shared_page_map_intern(vaddr_t *pgaddr, int *unused, spdid_t cur, int idx)
{
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cap_shmem_info *cur_shi = cap_info_shmem_info(cur_rci);
	int num_pages = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci)) return 0;
	if (!cur_shi) return 0;

	num_pages = cap_shmem_region_map(cur_shi, idx);
	if (num_pages == 0) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, idx);
	assert(*pgaddr);

done:
	return num_pages;
}
