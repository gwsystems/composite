/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <memmgr.h>
#include <cap_info.h>

vaddr_t
memmgr_va2pa(vaddr_t vaddr)
{
	spdid_t cur = cos_inv_token();
	struct cos_compinfo *cap_ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cap_comp_info *cur_rci = cap_info_comp_find(cur);
	struct cos_compinfo *cur_ci = cap_info_ci(cur_rci);

	return (vaddr_t)cos_va2pa(cur_ci, (void *)vaddr);
}

vaddr_t
memmgr_pa2va_map(paddr_t pa, unsigned int len)
{
	spdid_t cur = cos_inv_token();
	struct cap_comp_info *cur_rci = cap_info_comp_find(cur);
	struct cos_compinfo *cur_ci = cap_info_ci(cur_rci);
	vaddr_t va = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci) || !cur_ci) return 0;

	va = (vaddr_t)cos_hw_map(cur_ci, BOOT_CAPTBL_SELF_INITHW_BASE, pa, len);

	return va;
}

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

	shmid = cap_shmem_region_alloc(cur_shi, 0, npages);
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
	cbuf_t tmpid;

	if (!cur_rci || !cap_info_init_check(cur_rci)) return 0;
	if (!cur_shi) return 0;

	tmpid = cap_shmem_region_map(cur_shi, id, 0, &num_pages);
	if (tmpid != id || num_pages == 0) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, id);
	assert(*pgaddr);

done:
	return num_pages;
}

/* These functions return the address at which the gs register is set to */
void *
memmgr_tls_alloc(unsigned int dst_tid)
{
	void *ret;

	/* Just return the vaddr range for this tid */
	assert(dst_tid < MAX_NUM_THREADS);
	/* have per-core tls regions */
	ret = TLS_BASE_ADDR + (TLS_AREA_SIZE * MAX_NUM_CPU_THDS * cos_cpuid()) + (TLS_AREA_SIZE * dst_tid);

	return ret;
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
	tid        = cos_introspect(cur_ci, dst_thdcap, THD_GET_TID, 0);
	addr       = memmgr_tls_alloc(tid);
	assert(addr);

	cos_thd_mod(rci, dst_thdcap, addr);

	return addr;
}
