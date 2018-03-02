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

	HYPERCALL_COMP_INITTHD_GET,
	HYPERCALL_COMP_CHILDREN_GET,
	HYPERCALL_COMP_SCHED_CHILDREN_GET,

	HYPERCALL_NUMCOMPS_GET,
};

/* assumption: spdids are monotonically increasing from 0 and max MAX_NUM_SPD == 64 */
static inline int
hypercall_comp_children_get(spdid_t c, u64_t *child_bitf)
{
	word_t lo = 0, hi = 0;
	int ret;

	*child_bitf = 0;
	ret         = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_CHILDREN_GET, c, 0, &lo, &hi);
	*child_bitf = ((u64_t)hi << 32) | (u64_t)lo;

	return ret;
}

static inline int
hypercall_comp_sched_children_get(spdid_t c, u64_t *child_bitf)
{
	word_t lo = 0, hi = 0;
	int ret;

	*child_bitf = 0;
	ret         = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_SCHED_CHILDREN_GET, c, 0, &lo, &hi);
	*child_bitf = ((u64_t)hi << 32) | (u64_t)lo;

	return ret;
}

static inline int
hypercall_comp_init_done(void)
{
	/*
	 * to be used only by the booter child threads
	 * higher-level components use, schedinit interface to SINV to parent for init
	 */
	return cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INIT_DONE, 0, 0);
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline int
hypercall_comp_initthd_get(spdid_t spdid, int is_sched, thdcap_t *thdslot, arcvcap_t *rcvslot, tcap_t *tcslot)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());

	*thdslot = cos_capid_bump_alloc(ci, CAP_THD);
	assert(*thdslot);

	if (is_sched) {
		*rcvslot = cos_capid_bump_alloc(ci, CAP_ARCV);
		assert(*rcvslot);

		*tcslot = cos_capid_bump_alloc(ci, CAP_TCAP);
		assert(*tcslot);
	}

	/* capid_t though is unsigned long, only assuming it occupies 16bits for packing */
	return cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INITTHD_GET,
			      spdid << 16 | (*thdslot), (*rcvslot) << 16 | (*tcslot));
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
	ret = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INFO_GET,
			     spdid << 16 | (*compslot), (*ptslot) << 16 | (*ctslot), &r2, &r3);
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
	ret =  cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INFO_NEXT,
			      (*compslot), (*ptslot) << 16 | (*ctslot), &r2, &r3);
	*compid        = r2;
	*comp_parentid = r3;

	return ret;

}

static inline int
hypercall_comp_frontier_get(spdid_t spdid, vaddr_t *vasfr, capid_t *capfr)
{
	return cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_FRONTIER_GET, spdid, 0, vasfr, capfr);
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline compcap_t
hypercall_comp_compcap_get(spdid_t spdid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	compcap_t compslot = cos_capid_bump_alloc(ci, CAP_COMP);

	assert(compslot);

	if (cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_COMPCAP_GET, spdid, compslot)) return 0;

	return compslot;
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline captblcap_t
hypercall_comp_captblcap_get(spdid_t spdid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	captblcap_t ctslot = cos_capid_bump_alloc(ci, CAP_CAPTBL);

	assert(ctslot);

	if (cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_CAPTBLCAP_GET, spdid, ctslot)) return 0;

	return ctslot;
}

/* Note: This API can be called ONLY by components that manage capability resources */
static inline pgtblcap_t
hypercall_comp_pgtblcap_get(spdid_t spdid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	pgtblcap_t ptslot = cos_capid_bump_alloc(ci, CAP_PGTBL);

	assert(ptslot);

	if (cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_PGTBLCAP_GET, spdid, ptslot)) return 0;

	return ptslot;
}

static inline capid_t
hypercall_comp_capfrontier_get(spdid_t spdid)
{
	word_t unused;
	capid_t cap_frontier;

	if (cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_CAPFRONTIER_GET, spdid, 0, &cap_frontier, &unused)) return 0;

	return cap_frontier;
}

static inline int
hypercall_numcomps_get(void)
{
	return cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_NUMCOMPS_GET, 0, 0);
}

#endif /* HYPERCALL_H */
