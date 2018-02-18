#ifndef RES_INFO_H
#define RES_INFO_H

#include <consts.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <resmgr.h>
#include <memmgr.h>

struct res_thd_info {
	int init_data_off;
	struct sl_thd *schthd;
};

struct res_shmem_region_info {
	vaddr_t region;
	vaddr_t res_region;
	int num_pages;
};

struct res_shmem_info {
	int free_idx, total_pages;
	struct cos_compinfo shcinfo;	

	struct res_shmem_region_info shmdata[MEMMGR_COMP_MAX_SHREGION];
};

struct res_comp_info {
	spdid_t cid;
	int thd_used;
	struct cos_defcompinfo defci;
	struct res_shmem_info shminfo;
	struct res_thd_info tinfo[MAX_NUM_THREADS];
	int initflag;
	u64_t chbits; /* all child components */
	u64_t chschbits; /* TODO: child components which are also schedulers. NOT IMPL Yet! */

	struct res_comp_info *parent;
};

struct res_comp_info *res_info_comp_init(spdid_t sid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
					 capid_t cap_frontier, vaddr_t heap_frontier, vaddr_t shared_frontier, 
					 spdid_t par_sid, u64_t ch, u64_t ch_sch);

struct res_thd_info *res_info_thd_init(struct res_comp_info *rci, struct sl_thd *t);
struct res_thd_info *res_info_initthd_init(struct res_comp_info *rci, struct sl_thd *t);

struct res_comp_info *res_info_comp_find(spdid_t s);
struct res_thd_info *res_info_thd_find(struct res_comp_info *r, thdid_t t);
struct res_thd_info *res_info_initthd(struct res_comp_info *r);
unsigned int res_info_count(void);
void res_info_init(void);

int res_shmem_region_alloc(struct res_shmem_info *rsh, int num_pages);
int res_shmem_region_map(struct res_shmem_info *rsh, struct res_shmem_info *rsh_src, int srcidx, int off, int num_pages);
vaddr_t res_shmem_region_comp_vaddr(struct res_shmem_info *rsh, int idx);
vaddr_t res_shmem_region_res_vaddr(struct res_shmem_info *rsh, int idx);

static inline struct res_shmem_region_info *
res_shmem_region_info(struct res_shmem_info *rsh, int idx)
{
	return &(rsh->shmdata[idx]);
}

#define IS_CHILDBIT_SET(r, c) (r->chbits & ((u64_t)1 << c))
#define IS_CHILDSCHEDBIT_SET(r, c) (r->chschbits & ((u64_t)1 << c))

static inline struct cos_compinfo *
res_info_ci(struct res_comp_info *r)
{
	return cos_compinfo_get(&(r->defci));
}

static inline struct cos_defcompinfo *
res_info_dci(struct res_comp_info *r)
{
	return &(r->defci);
}

static inline struct res_comp_info *
res_info_parent(struct res_comp_info *r)
{
	return r->parent;
}

static inline int
res_info_is_parent(struct res_comp_info *r, spdid_t p)
{
	if (res_info_parent(r)) return (r->parent->cid == p);

	return 0;
}

static inline int
res_info_is_child(struct res_comp_info *r, spdid_t c)
{
	return IS_CHILDBIT_SET(r, c);
}

static inline int
res_info_is_sched_child(struct res_comp_info *r, spdid_t c)
{
	return IS_CHILDSCHEDBIT_SET(r, c);
}

static inline struct res_shmem_info *
res_info_shmem_info(struct res_comp_info *r)
{
	return &(r->shminfo);
}

static inline struct cos_compinfo *
res_info_shmem_ci(struct res_shmem_info *r)
{
	return &(r->shcinfo);
}

static inline int
res_info_init_check(struct res_comp_info *r)
{
	return r->initflag;
}

static inline thdcap_t
res_thd_thdcap(struct res_thd_info *ti)
{
	return sl_thd_thdcap(ti->schthd);
}

static inline arcvcap_t
res_thd_rcvcap(struct res_thd_info *ti)
{
	return sl_thd_rcvcap(ti->schthd);
}

static inline tcap_t
res_thd_tcap(struct res_thd_info *ti)
{
	return sl_thd_tcap(ti->schthd);
}

static inline asndcap_t
res_thd_asndcap(struct res_thd_info *ti)
{
	return sl_thd_asndcap(ti->schthd);
}

static inline thdcap_t
res_thd_thdid(struct res_thd_info *ti)
{
	return ti->schthd->thdid;
}

#endif /* RES_INFO_H */
