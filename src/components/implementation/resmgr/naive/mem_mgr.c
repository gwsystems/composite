#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <memmgr.h>
#include "res_info.h"

vaddr_t
memmgr_heap_page_allocn_intern(spdid_t cur, unsigned int npages)
{
	struct cos_compinfo  *res_ci  = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct res_comp_info *cur_rci = res_info_comp_find(cur);
	struct cos_compinfo  *cur_ci  = res_info_ci(cur_rci);
	vaddr_t src_pg, dst_pg, first_pg;
	unsigned int off = 0;

	if (!cur_rci || !res_info_init_check(cur_rci)) return 0;
	if (!cur_ci) return 0;

	src_pg = (vaddr_t)cos_page_bump_allocn(res_ci, npages * PAGE_SIZE);
	if (!src_pg) return 0;

	while (off < npages) {
		dst_pg = cos_mem_alias(cur_ci, res_ci, src_pg + (off * PAGE_SIZE));
		if (!dst_pg) return 0;

		if (!off) first_pg = dst_pg;
		off++;
	}

	return first_pg;
}

int
memmgr_shared_page_allocn_intern(spdid_t cur, int num, int u1, int u2, vaddr_t *pgaddr, int *u3)
{
	struct res_comp_info  *cur_rci = res_info_comp_find(cur);
	struct res_shmem_info *cur_shi = res_info_shmem_info(cur_rci);
	int shmidx = -1;

	if (!cur_rci || !res_info_init_check(cur_rci)) return 0;
	if (!cur_shi) return 0;

	shmidx = res_shmem_region_alloc(cur_shi, num);
	if (shmidx < 0) goto done;

	*pgaddr = res_shmem_region_vaddr(cur_shi, shmidx);

done:
	return shmidx;
}

int
memmgr_shared_page_map_intern(spdid_t cur, int idx, int u1, int u2, vaddr_t *pgaddr, int *u3)
{
	struct res_comp_info  *cur_rci = res_info_comp_find(cur);
	struct res_shmem_info *cur_shi = res_info_shmem_info(cur_rci);
	int num_pages = 0;

	if (!cur_rci || !res_info_init_check(cur_rci)) return 0;
	if (!cur_shi) return 0;

	num_pages = res_shmem_region_map(cur_shi, idx);
	if (num_pages == 0) goto done;

	*pgaddr = res_shmem_region_vaddr(cur_shi, idx);
	assert(*pgaddr);

done:
	return num_pages;
}
