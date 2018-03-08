#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <capmgr.h>
#include "cap_info.h"
#include <hypercall.h>
#include <sl.h>

static void
capmgr_comp_info_iter(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	struct cap_comp_info   *btinfo = cap_info_comp_find(0);
	int remaining = 0;
	int num_comps = 0;

	do {
		spdid_t csid = 0, psid = 0;
		thdcap_t thdslot = 0;
		arcvcap_t rcvslot = 0;
		tcap_t tcslot = 0;
		struct cap_comp_info *rci = NULL;
		struct sl_thd *ithd = NULL;
		u64_t chbits = 0, chschbits = 0;
		pgtblcap_t pgtslot = 0;
		captblcap_t captslot = 0;
		compcap_t ccslot = 0;
		vaddr_t vasfr = 0;
		capid_t capfr = 0;
		int ret = 0, is_sched = 0;
		int remain_child = 0;
		spdid_t childid;
		comp_flag_t ch_flags;

		remaining = hypercall_comp_info_next(&pgtslot, &captslot, &ccslot, &csid, &psid);
		if (remaining < 0) {
			assert(remaining == -1); /* iterator end */
			break;
		}

		num_comps ++;
		while ((remain_child = hypercall_comp_child_next(csid, &childid, &ch_flags)) >= 0) {
			chbits |= (1 << (childid-1));
			if (ch_flags & COMP_FLAG_SCHED) chschbits |= (1 << (childid-1));

			if (!remain_child) break;
		}
		cap_info_schedbmp |= chschbits;

		if (csid == 0 || (csid != cos_spd_id() && cap_info_is_child(btinfo, csid))) {
			is_sched = (csid == 0 || cap_info_is_sched_child(btinfo, csid)) ? 1 : 0;

			ret = hypercall_comp_initthd_get(csid, is_sched, &thdslot, &rcvslot, &tcslot);
			assert(ret == 0);
		}

		ret = hypercall_comp_frontier_get(csid, &vasfr, &capfr);
		assert(ret == 0);

		rci = cap_info_comp_init(csid, captslot, pgtslot, ccslot, capfr, vasfr, (vaddr_t)MEMMGR_SHMEM_BASE, psid, chbits, chschbits);
		assert(rci);

		if (thdslot) {
			ithd = sl_thd_ext_init(thdslot, tcslot, rcvslot, 0);
			assert(ithd);

			cap_info_initthd_init(rci, ithd);
		} else if (cos_spd_id() == csid) {
			cap_info_initthd_init(rci, sl__globals()->sched_thd);
		}
	} while (remaining > 0);

	assert(num_comps == hypercall_numcomps_get());
	PRINTC("Schedulers bitmap: %llx\n", cap_info_schedbmp);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	capid_t cap_frontier = 0;
	vaddr_t heap_frontier = 0;
	spdid_t child;
	comp_flag_t ch_flags;
	int ret = 0;

	PRINTC("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));
	ret = hypercall_comp_frontier_get(cos_spd_id(), &heap_frontier, &cap_frontier);
	assert(ret == 0);

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_BASE, BOOT_CAPTBL_SELF_INITTHD_BASE,
				 BOOT_CAPTBL_SELF_INITRCV_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
				 BOOT_CAPTBL_SELF_COMP, heap_frontier, cap_frontier);

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &ch_flags) == -1);

	sl_init(SL_MIN_PERIOD_US);
	cap_info_init();
	capmgr_comp_info_iter();

	PRINTC("Initialized CAPABILITY MANAGER\n");

	hypercall_comp_init_done();
	PRINTC("SPINNING\n");
	while (1) ;
}
