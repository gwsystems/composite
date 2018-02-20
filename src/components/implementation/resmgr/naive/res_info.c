#include <cos_kernel_api.h>
#include "res_info.h"

static struct res_comp_info resci[MAX_NUM_COMPS + 1]; /* like booter => incl booter MAX + 1 */
static unsigned int res_comp_count;
u64_t res_info_schedbmp;
static struct res_shmem_glb_info res_shmglbinfo;
extern spdid_t resmgr_myspdid;

static inline struct res_shmem_glb_info *
__res_info_shmglb_info(void)
{
	return &res_shmglbinfo;
}

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

struct res_thd_info *
res_info_thd_next(struct res_comp_info *rci)
{
	assert(rci && rci->initflag);

	if (rci->p_thd_iterator < rci->thd_used) {
		return &(rci->tinfo[__sync_fetch_and_add(&(rci->p_thd_iterator), 1)]);
	}

	return NULL;
}

struct res_comp_info *
res_info_comp_init(spdid_t sid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
		   capid_t cap_frontier, vaddr_t heap_frontier, vaddr_t shared_frontier, spdid_t psid,
		   u64_t childbits, u64_t childschedbits)
{
	struct cos_compinfo       *ci      = cos_compinfo_get(&(resci[sid].defci));
	struct res_shmem_info     *res_shi = res_info_shmem_info(&resci[sid]);
	struct cos_compinfo       *sh_ci   = res_info_shmem_ci(res_shi);
	struct res_shmem_glb_info *rglb    = __res_info_shmglb_info();

	resci[sid].cid       = sid;
	resci[sid].thd_used  = 1;
	resci[sid].parent    = &resci[psid];

	resci[sid].chbits = childbits;
	resci[sid].chschbits = childschedbits;

	cos_meminfo_init(&ci->mi, 0, 0, 0);
	cos_compinfo_init(ci, pgtbl_cap, captbl_cap, compcap, heap_frontier, cap_frontier, 
			  cos_compinfo_get(cos_defcompinfo_curr_get()));

	memset(rglb, 0, sizeof(struct res_shmem_glb_info));
	memset(res_shi, 0, sizeof(struct res_shmem_info));
	cos_meminfo_init(&sh_ci->mi, 0, 0, 0);
	cos_compinfo_init(sh_ci, pgtbl_cap, 0, 0, shared_frontier, 0, 
			  cos_compinfo_get(cos_defcompinfo_curr_get()));

	resci[sid].initflag = 1;
	__sync_fetch_and_add(&res_comp_count, 1);

	return &resci[sid];
}

struct res_thd_info *
res_info_thd_init(struct res_comp_info *rci, struct sl_thd *t)
{
	int off;

	assert(rci && rci->initflag);
	assert(rci->thd_used < MAX_NUM_THREADS-1);
	assert(t);

	off = __sync_fetch_and_add(&(rci->thd_used), 1);

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
	res_info_schedbmp = 0;
	memset(resci, 0, sizeof(struct res_comp_info)*(MAX_NUM_COMPS+1));
}

static inline vaddr_t
__res_info_shm_resmgr_vaddr(int id)
{
	return resci[resmgr_myspdid].shminfo.shm_addr[id];
}

static inline void 
__res_info_shm_resmgr_vaddr_set(int id, vaddr_t v)
{
	resci[resmgr_myspdid].shminfo.shm_addr[id] = v;
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
	struct cos_compinfo       *rsh_ci = res_info_shmem_ci(rsh);
	struct res_shmem_glb_info *rglb   = __res_info_shmglb_info();
	int alloc_idx = -1, fidx, ret;
	vaddr_t res_addr, comp_addr;

	assert(rsh);
	
	/* limits check */
	if ((rglb->total_pages + num_pages) * PAGE_SIZE > MEMMGR_MAX_SHMEM_SIZE) goto done;
	fidx = __sync_fetch_and_add(&(rglb->free_region_id), 1);
	if (fidx >= MEMMGR_MAX_SHMEM_REGIONS) goto done;

	/* check id unused */
	assert(__res_info_shm_resmgr_vaddr(fidx) == 0);
	assert(rsh->shm_addr[fidx] == 0);

	rglb->npages[fidx] = num_pages;
	__sync_fetch_and_add(&(rglb->total_pages), num_pages);

	ret = __res_cos_shared_page_allocn(rsh_ci, num_pages, &res_addr, &comp_addr);
	assert(!ret);
	__res_info_shm_resmgr_vaddr_set(fidx, res_addr);
	rsh->shm_addr[fidx] = comp_addr;
	alloc_idx = fidx;
done:
	return alloc_idx;
}

int
res_shmem_region_map(struct res_shmem_info *rsh, int idx)
{
	struct cos_compinfo       *rsh_ci = res_info_shmem_ci(rsh);
	struct res_shmem_glb_info *rglb   = __res_info_shmglb_info();
	vaddr_t res_addr = __res_info_shm_resmgr_vaddr(idx), comp_addr;
	int ret = -1;

	assert(rsh);
	assert(idx < MEMMGR_MAX_SHMEM_REGIONS);
	assert(res_addr && rsh->shm_addr[idx] == 0);

	ret = __res_cos_shared_page_mapn(rsh_ci, rglb->npages[idx], res_addr, &comp_addr);
	assert(!ret);
	rsh->shm_addr[idx] = comp_addr;

	return rglb->npages[idx];
}

vaddr_t
res_shmem_region_vaddr(struct res_shmem_info *rsh, int id)
{
	return rsh->shm_addr[id];
}
