#ifndef RES_INFO_H
#define RES_INFO_H

#include <consts.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>

struct res_thd_info {
	int init_data_off;
	struct sl_thd *schthd;
};

struct res_comp_info {
	spdid_t cid;
	int sched_off;
	int thd_used;
	struct cos_defcompinfo defci;
	struct res_thd_info tinfo[MAX_NUM_THREADS];
	int initflag;

	struct res_comp_info *parent;
};

struct res_comp_info *res_info_comp_init(spdid_t sid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
					 capid_t cap_frontier, vaddr_t heap_frontier, spdid_t par_sid);

struct res_thd_info *res_info_thd_init(struct res_comp_info *rci, struct sl_thd *t);
struct res_thd_info *res_info_initthd_init(struct res_comp_info *rci, struct sl_thd *t);

struct res_comp_info *res_info_comp_find(spdid_t s);
struct res_thd_info *res_info_thd_find(struct res_comp_info *r, thdid_t t);
struct res_thd_info *res_info_initthd(struct res_comp_info *r);
unsigned int res_info_count(void);
void res_info_init(void);

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
