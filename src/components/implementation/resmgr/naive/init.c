#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <resmgr.h>
#include "res_info.h"
#include <hypercall.h>
#include <sl.h>

spdid_t resmgr_myspdid = 0;

static inline void
resmgr_capfr_update(capid_t cfr)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	cos_capfrontier_init(ci, cfr);
}

static void
resmgr_comp_info_iter(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	int ret = 0;

	do {
		pgtblcap_t pgc;
		captblcap_t capc;
		compcap_t cc;
		spdid_t csid, psid;
		vaddr_t vasfr;
		capid_t capfr, rescapfr = ci->cap_frontier;
		thdcap_t t = 0;
		arcvcap_t rcv = 0;
		tcap_t tc = 0;
		struct res_comp_info *rci;
		struct sl_thd *ithd;
		u64_t chbits = 0, chschbits = 0;
	
		ret = hypercall_comp_info_next(rescapfr, &csid, &pgc, &capc, &cc, &psid, &rescapfr);
		if (ret) break;

		resmgr_capfr_update(rescapfr);
		if (resmgr_myspdid != csid) t = hypercall_comp_initthd_get(csid, &rcv, &tc, &rescapfr);
		if (t) resmgr_capfr_update(rescapfr);

		ret = hypercall_comp_frontier_get(csid, &vasfr, &capfr);
		assert(ret == 0);

		ret = hypercall_comp_childspdids_get(csid, &chbits);
		assert(ret == 0);
		ret = hypercall_comp_childschedspdids_get(csid, &chschbits);
		assert(ret == 0);
		res_info_schedbmp |= chschbits;

		rci = res_info_comp_init(csid, capc, pgc, cc, capfr, vasfr, (vaddr_t)MEMMGR_SHMEM_BASE, psid, chbits, chschbits);
		assert(rci);

		if (t) {
			ithd = sl_thd_ext_init(t, tc, rcv, 0);
			assert(ithd);

			res_info_initthd_init(rci, ithd);
		} else if (cos_spd_id() == csid) {
			res_info_initthd_init(rci, sl__globals()->sched_thd);
		}
	} while (ret == 0);

	PRINTC("Schedulers bitmap: %llx\n", res_info_schedbmp);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	u64_t childbits = 0;

	resmgr_myspdid = cos_spd_id();
	assert(resmgr_myspdid);
	PRINTC("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	hypercall_comp_childspdids_get(cos_spd_id(), &childbits);
	assert(!childbits);

	sl_init(SL_MIN_PERIOD_US);
	res_info_init();
	resmgr_comp_info_iter();

	PRINTC("Initialized RESOURCE MANAGER\n");

	hypercall_comp_init_done();
	PRINTC("SPINNING\n");
	while (1) ;
}
