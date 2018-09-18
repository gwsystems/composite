#ifndef HYPERCALL_H
#define HYPERCALL_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

enum hypercall_cntl {
	HYPERCALL_COMP_INIT_DONE = 0,
	HYPERCALL_COMP_INFO_GET, /* packed! <retval>, <pgtbl, captbl>, <compcap, parent_spdid> */
	HYPERCALL_COMP_INFO_NEXT, /* iterator to get comp_info */
	HYPERCALL_COMP_FRONTIER_GET, /* get current cap frontier & vaddr frontier of spdid comp */

	HYPERCALL_COMP_COMPCAP_GET,
	HYPERCALL_COMP_CAPTBLCAP_GET,
	HYPERCALL_COMP_PGTBLCAP_GET,
	HYPERCALL_COMP_CAPFRONTIER_GET,

	HYPERCALL_COMP_INITAEP_GET,
	HYPERCALL_COMP_CHILD_NEXT,
	HYPERCALL_COMP_CPUBITMAP_GET,

	HYPERCALL_NUMCOMPS_GET,
};

static inline int
hypercall_comp_child_next(spdid_t c, spdid_t *child, comp_flag_t *flags)
{
	word_t r2 = 0, r3 = 0;
	int ret;

	ret = cos_sinv_rets(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_CHILD_NEXT, c, 0, 0, &r2, &r3);
	if (ret < 0) return ret;
	*child = (spdid_t)r2;
	*flags = (comp_flag_t)r3;

	return ret; /* remaining child spds */
}

static inline int
hypercall_comp_init_done(void)
{
	/*
	 * to be used only by the booter child threads
	 * higher-level components use, schedinit interface to SINV to parent for init
	 */
	return cos_sinv(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_INIT_DONE, 0, 0, 0);
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline int
hypercall_comp_initaep_get(spdid_t spdid, int is_sched, struct cos_aep_info *aep)
{
	thdcap_t  thdslot = 0;
	arcvcap_t rcvslot = 0;
	tcap_t    tcslot  = 0;
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	int ret = 0;

	thdslot = cos_capid_bump_alloc(ci, CAP_THD);
	assert(thdslot);

	if (is_sched) {
		rcvslot = cos_capid_bump_alloc(ci, CAP_ARCV);
		assert(rcvslot);

		tcslot = cos_capid_bump_alloc(ci, CAP_TCAP);
		assert(tcslot);
	}

	/* capid_t though is unsigned long, only assuming it occupies 16bits for packing */
	ret = cos_sinv(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_INITAEP_GET,
			spdid << 16 | thdslot, rcvslot << 16 | tcslot, 0);
	if (ret) return ret;

	aep->thd = thdslot;
	aep->rcv = rcvslot;
	aep->tc  = tcslot;
	aep->tid = cos_introspect(ci, thdslot, THD_GET_TID);

	return 0;
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline int
hypercall_comp_info_get(spdid_t spdid, pgtblcap_t *ptslot, captblcap_t *ctslot, compcap_t *compslot, spdid_t *parentid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	word_t r2 = 0, r3 = 0;
	int ret = 0;

	*ptslot   = cos_capid_bump_alloc(ci, CAP_PGTBL);
	assert(*ptslot);
	*ctslot   = cos_capid_bump_alloc(ci, CAP_CAPTBL);
	assert(*ctslot);
	*compslot = cos_capid_bump_alloc(ci, CAP_COMP);
	assert(*compslot);

	/* capid_t though is unsigned long, only assuming it occupies 16bits for packing */
	ret = cos_sinv_rets(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_INFO_GET,
			     spdid << 16 | (*compslot), (*ptslot) << 16 | (*ctslot), 0, &r2, &r3);
	*parentid = r2;

	return ret;
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline int
hypercall_comp_info_next(pgtblcap_t *ptslot, captblcap_t *ctslot, compcap_t *compslot, spdid_t *compid, spdid_t *comp_parentid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	word_t r2 = 0, r3 = 0;
	int ret = 0;

	*ptslot   = cos_capid_bump_alloc(ci, CAP_PGTBL);
	assert(*ptslot);
	*ctslot   = cos_capid_bump_alloc(ci, CAP_CAPTBL);
	assert(*ctslot);
	*compslot = cos_capid_bump_alloc(ci, CAP_COMP);
	assert(*compslot);

	/* capid_t though is unsigned long, only assuming it occupies 16bits for packing */
	ret =  cos_sinv_rets(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_INFO_NEXT,
			      (*compslot), (*ptslot) << 16 | (*ctslot), 0, &r2, &r3);
	*compid        = r2;
	*comp_parentid = r3;

	return ret;

}

static inline int
hypercall_comp_frontier_get(spdid_t spdid, vaddr_t *vasfr, capid_t *capfr)
{
	return cos_sinv_rets(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_FRONTIER_GET, spdid, 0, 0, vasfr, capfr);
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline compcap_t
hypercall_comp_compcap_get(spdid_t spdid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	compcap_t compslot = cos_capid_bump_alloc(ci, CAP_COMP);

	assert(compslot);

	if (cos_sinv(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_COMPCAP_GET, spdid, compslot, 0)) return 0;

	return compslot;
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline captblcap_t
hypercall_comp_captblcap_get(spdid_t spdid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	captblcap_t ctslot = cos_capid_bump_alloc(ci, CAP_CAPTBL);

	assert(ctslot);

	if (cos_sinv(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_CAPTBLCAP_GET, spdid, ctslot, 0)) return 0;

	return ctslot;
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline pgtblcap_t
hypercall_comp_pgtblcap_get(spdid_t spdid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	pgtblcap_t ptslot = cos_capid_bump_alloc(ci, CAP_PGTBL);

	assert(ptslot);

	if (cos_sinv(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_PGTBLCAP_GET, spdid, ptslot, 0)) return 0;

	return ptslot;
}

static inline capid_t
hypercall_comp_capfrontier_get(spdid_t spdid)
{
	word_t unused;
	capid_t cap_frontier;

	if (cos_sinv_rets(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_CAPFRONTIER_GET, spdid, 0, 0, &cap_frontier, &unused)) return 0;

	return cap_frontier;
}

static inline int
hypercall_comp_cpubitmap_get(spdid_t spdid, u32_t *bmp)
{
	word_t hi = 0, lo = 0;

	assert(NUM_CPU_BMP_WORDS <= 2); /* FIXME: works for up to 64 cores */

	if (cos_sinv_rets(BOOT_CAPTBL_SINV_CAP, HYPERCALL_COMP_CPUBITMAP_GET, spdid, 0, 0, &lo, &hi)) return -1;

	bmp[0] = lo;
	if (NUM_CPU_BMP_WORDS == 2) bmp[1] = hi;

	return 0;
}

static inline int
hypercall_numcomps_get(void)
{
	return cos_sinv(BOOT_CAPTBL_SINV_CAP, HYPERCALL_NUMCOMPS_GET, 0, 0, 0);
}

#endif /* HYPERCALL_H */
