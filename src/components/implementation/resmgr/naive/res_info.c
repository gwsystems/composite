#include <cos_kernel_api.h>
#include "res_info.h"

static struct res_comp_info resci[MAX_NUM_COMPS];
static unsigned int res_comp_count;

struct res_comp_info *
res_info_comp_find(spdid_t sid)
{
	return &resci[sid];
}

unsigned int
res_info_count(void)
{
	return res_comp_count;
}

struct res_thd_info *
res_info_thd_find(struct res_comp_info *rci, thdid_t tid)
{
	int i = 0;

	assert(rci && rci->initflag);
	for (; i < rci->thd_used; i++) {
		if (((rci->tinfo[i]).schthd)->thdid == tid) return &(rci->tinfo[i]);
	}

	return NULL;
}

struct res_comp_info *
res_info_comp_init(spdid_t sid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
		   capid_t cap_frontier, vaddr_t heap_frontier, vaddr_t shared_frontier, spdid_t psid,
		   u64_t childbits, u64_t childschedbits)
{
	struct cos_compinfo   *ci      = cos_compinfo_get(&(resci[sid].defci));
	struct res_shmem_info *res_shi = res_info_shmem_info(&resci[sid]);
	struct cos_compinfo   *sh_ci   = res_info_shmem_ci(res_shi);

	resci[sid].cid       = sid;
	resci[sid].thd_used  = 1;
	resci[sid].parent    = &resci[psid];

	resci[sid].chbits = childbits;
	resci[sid].chschbits = childschedbits;

	cos_meminfo_init(&ci->mi, 0, 0, 0);
	cos_compinfo_init(ci, pgtbl_cap, captbl_cap, compcap, heap_frontier, cap_frontier, 
			  cos_compinfo_get(cos_defcompinfo_curr_get()));

	memset(res_shi, 0, sizeof(struct res_shmem_info));
	cos_meminfo_init(&sh_ci->mi, 0, 0, 0);
	cos_compinfo_init(sh_ci, pgtbl_cap, 0, 0, shared_frontier, 0, 
			  cos_compinfo_get(cos_defcompinfo_curr_get()));

	resci[sid].initflag  = 1;
	res_comp_count ++;

	return &resci[sid];
}

struct res_thd_info *
res_info_thd_init(struct res_comp_info *rci, struct sl_thd *t)
{
	int off;

	assert(rci && rci->initflag);
	assert(rci->thd_used < MAX_NUM_THREADS-1);
	assert(t);

	off = rci->thd_used;
	rci->thd_used ++;

	rci->tinfo[off].schthd = t;

	return &(rci->tinfo[off]);
}

struct res_thd_info *
res_info_initthd_init(struct res_comp_info *rci, struct sl_thd *t)
{
	assert(rci && rci->initflag);
	assert(rci->thd_used < MAX_NUM_THREADS-1);
	assert(t);

	rci->tinfo[0].schthd = t;

	return &(rci->tinfo[0]);

}

struct res_thd_info *
res_info_initthd(struct res_comp_info *rci)
{
	assert(rci);

	return &(rci->tinfo[0]);
}

void
res_info_init(void)
{
	res_comp_count = 0;
	memset(resci, 0, sizeof(struct res_comp_info)*MAX_NUM_COMPS);
}

static int
__res_cos_shared_page_allocn(struct cos_compinfo *rci, int num_pages, vaddr_t *resvaddr, vaddr_t *compvaddr)
{
	struct cos_compinfo *res_ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	int off = 0;
	vaddr_t src_pg, dst_pg;

	*resvaddr = src_pg = (vaddr_t)cos_page_bump_allocn(res_ci, num_pages * PAGE_SIZE);
	assert(*resvaddr);

	while (off < num_pages) {
		dst_pg = cos_mem_alias(rci, res_ci, src_pg + (off * PAGE_SIZE));
		assert(dst_pg);

		if (!off) *compvaddr = dst_pg;
		off++;
	}

	return 0;
}

static int
__res_cos_shared_page_mapn(struct cos_compinfo *rci, int num_pages, vaddr_t resvaddr, vaddr_t *compvaddr)
{
	struct cos_compinfo *res_ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	vaddr_t dst_pg;
	int off = 0;

	assert(resvaddr);

	while (off < num_pages) {
		dst_pg = cos_mem_alias(rci, res_ci, resvaddr + (off * PAGE_SIZE));
		assert(dst_pg);

		if (!off) *compvaddr = dst_pg;
		off++;
	}

	return 0;
}

int
res_shmem_region_alloc(struct res_shmem_info *rsh, int num_pages)
{
	struct cos_compinfo *rsh_ci = res_info_shmem_ci(rsh);
	int alloc_idx = -1, fidx, ret;
	vaddr_t res_addr, comp_addr;

	/* TODO: lock-free alloc */
	assert(rsh);
	fidx = rsh->free_idx;
	if (fidx >= MEMMGR_COMP_MAX_SHREGION) goto done;
	if (rsh->total_pages + num_pages >= MEMMGR_COMP_MAX_SHREGION) goto done; 
	(rsh->free_idx) ++;
	rsh->total_pages += num_pages;

	ret = __res_cos_shared_page_allocn(rsh_ci, num_pages, &res_addr, &comp_addr);
	assert(!ret);

	rsh->shmdata[fidx].region     = comp_addr;
	rsh->shmdata[fidx].res_region = res_addr;
	rsh->shmdata[fidx].num_pages  = num_pages;

	alloc_idx = fidx;
done:
	return alloc_idx;
}

vaddr_t
res_shmem_region_comp_vaddr(struct res_shmem_info *rsh, int idx)
{
	assert(rsh && idx < rsh->free_idx);

	return res_shmem_region_info(rsh, idx)->region;
}

vaddr_t
res_shmem_region_res_vaddr(struct res_shmem_info *rsh, int idx)
{
	assert(rsh && idx < rsh->free_idx);

	return res_shmem_region_info(rsh, idx)->res_region;
}

int
res_shmem_region_map(struct res_shmem_info *rsh, struct res_shmem_info *rsh_src, int srcidx, int off, int num_pages)
{
	struct cos_compinfo *res_ci  = cos_compinfo_get(cos_defcompinfo_curr_get());	
	struct cos_compinfo *rsh_ci  = res_info_shmem_ci(rsh);
	struct res_shmem_region_info *regioninfo = res_shmem_region_info(rsh_src, srcidx);
	int alloc_idx = -1, fidx, ret;
	vaddr_t res_addr, comp_addr;

	/* TODO: lock-free alloc */
	assert(rsh);
	if (!num_pages) num_pages = regioninfo->num_pages;
	fidx = rsh->free_idx;
	if (fidx >= MEMMGR_COMP_MAX_SHREGION) goto done;
	if (regioninfo->num_pages < off + num_pages) goto done;
	if (rsh->total_pages + num_pages >= MEMMGR_COMP_MAX_SHREGION) goto done; 
	(rsh->free_idx) ++;
	rsh->total_pages += num_pages;

	res_addr = res_shmem_region_res_vaddr(rsh_src, srcidx);
	assert(res_addr);
	res_addr *= (off * PAGE_SIZE);

	ret = __res_cos_shared_page_mapn(rsh_ci, num_pages, res_addr, &comp_addr);
	assert(!ret);

	rsh->shmdata[fidx].region     = comp_addr;
	rsh->shmdata[fidx].res_region = res_addr;
	rsh->shmdata[fidx].num_pages  = num_pages;

	alloc_idx = fidx;
done:
	return alloc_idx;
}
