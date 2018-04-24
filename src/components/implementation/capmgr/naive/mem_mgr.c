#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <memmgr.h>
#include <cap_info.h>

vaddr_t
memmgr_heap_page_allocn(unsigned long npages)
{
	spdid_t cur = cos_inv_token();
	struct cos_compinfo  *cap_ci  = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cap_comp_info *cur_rci = cap_info_comp_find(cur);
	struct cos_compinfo  *cur_ci  = cap_info_ci(cur_rci);
	vaddr_t src_pg, dst_pg;

	if (!cur_rci || !cap_info_init_check(cur_rci)) return 0;
	if (!cur_ci) return 0;

	src_pg = (vaddr_t)cos_page_bump_allocn(cap_ci, npages * PAGE_SIZE);
	if (!src_pg) return 0;
	dst_pg = cos_mem_aliasn(cur_ci, cap_ci, src_pg, npages * PAGE_SIZE);

	return dst_pg;
}

cbuf_t
memmgr_shared_page_allocn_cserialized(vaddr_t *pgaddr, int *unused, unsigned long npages)
{
	spdid_t cur = cos_inv_token();
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cap_shmem_info *cur_shi = cap_info_shmem_info(cur_rci);
	cbuf_t shmid = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci)) goto done;
	if (!cur_shi) goto done;

	shmid = cap_shmem_region_alloc(cur_shi, npages);
	if (!shmid) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, shmid);

done:
	return shmid;
}

unsigned long
memmgr_shared_page_map_cserialized(vaddr_t *pgaddr, int *unused, cbuf_t id)
{
	spdid_t cur = cos_inv_token();
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cap_shmem_info *cur_shi = cap_info_shmem_info(cur_rci);
	unsigned long num_pages = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci)) return 0;
	if (!cur_shi) return 0;

	num_pages = cap_shmem_region_map(cur_shi, id);
	if (num_pages == 0) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, id);
	assert(*pgaddr);

done:
	return num_pages;
}

/* These functions return the address at which the gs register is set to */
void *
memmgr_tls_alloc(unsigned int dst_tid)
{
	/* Just return the vaddr range for this tid */
	assert(dst_tid < MAX_NUM_THREADS);
	return TLS_BASE_ADDR + (void *)(TLS_AREA_SIZE * dst_tid);
}

void *
_memmgr_tls_alloc_and_set(void *area)
{
	spdid_t cur = cos_inv_token();
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cos_compinfo  *rci      = cap_info_ci(cur_rci);
	struct cos_compinfo  *cur_ci   = cos_compinfo_get(cos_defcompinfo_curr_get());
	void *addr;
	unsigned int dst_thdcap;
	int tid;

	assert(cur_ci && rci);

	dst_thdcap = sl_thd_curr()->aepinfo->thd;
	tid        = cos_introspect(cur_ci, dst_thdcap, THD_GET_TID);
	addr       = memmgr_tls_alloc(tid);
	assert(addr);

	cos_thd_mod(rci, dst_thdcap, addr);

	return addr;
}
