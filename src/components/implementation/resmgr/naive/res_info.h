#ifndef RES_INFO_H
#define RES_INFO_H

#include <consts.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <resmgr.h>
#include <memmgr.h>

extern u64_t res_info_schedbmp;

struct res_thd_info {
	int init_data_off;
	struct sl_thd *schthd;
};

struct res_shmem_glb_info {
	int free_region_id;
	int total_pages;

	int npages[MEMMGR_MAX_SHMEM_REGIONS];
};

struct res_shmem_info {
	struct cos_compinfo shcinfo;	

	vaddr_t shm_addr[MEMMGR_MAX_SHMEM_REGIONS];
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

int res_shmem_region_alloc(struct res_shmem_info *rcur, int num_pages);
int res_shmem_region_map(struct res_shmem_info *rcur, int id);
vaddr_t res_shmem_region_vaddr(struct res_shmem_info *rsh, int id);

#define IS_SCHEDBIT_SET(s, c) (s & ((u64_t)1 << (c-1)))
#define IS_CHILDBIT_SET(r, c) (r->chbits & ((u64_t)1 << (c-1)))
#define IS_CHILDSCHEDBIT_SET(r, c) (r->chschbits & ((u64_t)1 << (c-1)))

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
res_info_is_sched(spdid_t c)
{
	if (!c) return 1; /* llbooter! */
	return IS_SCHEDBIT_SET(res_info_schedbmp, c);
}

static inline int
res_info_is_child(struct res_comp_info *r, spdid_t c)
{
	if (!c) return 0;
	return IS_CHILDBIT_SET(r, c);
}

static inline int
res_info_is_sched_child(struct res_comp_info *r, spdid_t c)
{
	if (!c) return 0;
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
