#ifndef RES_INFO_H
#define RES_INFO_H

#include <consts.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <resmgr.h>
#include <memmgr.h>

#define RES_INFO_COMP_MAX_SUBTREE 8
#define RES_INFO_COMP_MAX_THREADS (MAX_NUM_THREADS*RES_INFO_COMP_MAX_SUBTREE)

extern u64_t res_info_schedbmp;

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
	struct sl_thd *tinfo[RES_INFO_COMP_MAX_THREADS]; /* including threads from components in subtree. */
	int initflag;
	u64_t chbits; /* all child components */
	u64_t chschbits; /* child components which are also schedulers. */

	struct res_comp_info *parent;
	int p_thd_iterator; /* iterator for parent to get all threads created by resmgr in this component so far! */
	thdcap_t p_initthdcap; /* init thread's cap in parent */
	thdid_t  initthdid; /* init thread's tid */
};

struct res_comp_info *res_info_comp_init(spdid_t sid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
					 capid_t cap_frontier, vaddr_t heap_frontier, vaddr_t shared_frontier, 
					 spdid_t par_sid, u64_t ch, u64_t ch_sch);

struct sl_thd *res_info_thd_init(struct res_comp_info *rci, struct sl_thd *t);
struct sl_thd *res_info_initthd_init(struct res_comp_info *rci, struct sl_thd *t);

struct res_comp_info *res_info_comp_find(spdid_t s);
struct sl_thd *res_info_thd_find(struct res_comp_info *r, thdid_t t);
struct sl_thd *res_info_thd_next(struct res_comp_info *r);
struct sl_thd *res_info_initthd(struct res_comp_info *r);
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

#endif /* RES_INFO_H */