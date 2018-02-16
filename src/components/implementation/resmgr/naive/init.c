#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <resmgr.h>
#include "res_info.h"
#include <llboot.h>
#include <sl.h>

void shdmem_init(void);

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
		struct res_comp_info *rci;
		struct sl_thd *ithd;
	
		ret = llboot_comp_info_next(rescapfr, &csid, &pgc, &capc, &cc, &psid, &rescapfr);
		if (ret) break;

		resmgr_capfr_update(rescapfr);
		if (cos_spd_id() != csid) t = llboot_comp_initthd_get(csid, &rescapfr);
		if (t) resmgr_capfr_update(rescapfr);

		ret = llboot_comp_frontier_get(csid, &vasfr, &capfr);
		assert(ret == 0);

		rci = res_info_comp_init(csid, capc, pgc, cc, capfr, vasfr, psid);
		assert(rci);

		if (t) {
			ithd = sl_thd_ext_init(t, 0, 0, 0);
			assert(ithd);

			res_info_initthd_init(rci, ithd);
		} else if (cos_spd_id() == csid) {
			res_info_initthd_init(rci, sl__globals()->sched_thd);
		}
	} while (ret == 0);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	printc("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	sl_init(SL_MIN_PERIOD_US);
	res_info_init();
	resmgr_comp_info_iter();

	printc("Initialized RESOURCE MANAGER\n");

	llboot_comp_init_done();
	printc("SPINNING\n");
	while (1) {
//		thdid_t tid;
//		int blocked;
//		cycles_t cycles;
//		tcap_time_t chtimeout;
//
//		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &chtimeout);
	}
}
