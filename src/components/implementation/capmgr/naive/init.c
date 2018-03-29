#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <capmgr.h>
#include <cap_info.h>
#include <hypercall.h>
#include <sl.h>

static int capmgr_init_core_done = 0;

static void
capmgr_comp_info_iter_cpu(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	struct cap_comp_info   *btinfo = cap_info_comp_find(0);
	int remaining = hypercall_numcomps_get(), i;
	int num_comps = 0;

	do {
		spdid_t spdid = num_comps, sched_spdid = 0;
		struct cap_comp_info *rci = cap_info_comp_find(spdid), *rci_sched = NULL;
		struct sl_thd *ithd = NULL;
		u64_t chbits = 0, chschbits = 0;
		int ret = 0, is_sched = 0;
		int remain_child = 0;
		spdid_t childid;
		comp_flag_t ch_flags;
		struct cos_aep_info aep;

		memset(&aep, 0, sizeof(struct cos_aep_info));
		assert(rci);
		assert(cap_info_init_check(rci));

		num_comps++;
		remaining--;
		if (spdid == 0 || (spdid != cos_spd_id() && cap_info_is_child(btinfo, spdid))) {
			is_sched = (spdid == 0 || cap_info_is_sched_child(btinfo, spdid)) ? 1 : 0;

			ret = hypercall_comp_initaep_get(spdid, is_sched, &aep);
			assert(ret == 0);
		}

		rci_sched = cap_info_comp_find(sched_spdid);
		assert(rci_sched && cap_info_init_check(rci_sched));
		rci->parent[cos_cpuid()] = rci_sched;
		rci->thd_used[cos_cpuid()] = 1;

		while ((remain_child = hypercall_comp_child_next(spdid, &childid, &ch_flags)) >= 0) {
			bitmap_set(rci->child_bitmap[cos_cpuid()], childid - 1);
			if (ch_flags & COMP_FLAG_SCHED) {
				bitmap_set(rci->child_sched_bitmap[cos_cpuid()], childid - 1);
				bitmap_set(cap_info_schedbmp[cos_cpuid()], childid - 1);
			}

			if (!remain_child) break;
		}

		if (aep.thd) {
			ithd = sl_thd_init_ext(&aep, NULL);
			assert(ithd);

			cap_info_initthd_init(rci, ithd, 0);
		} else if (cos_spd_id() == spdid) {
			cap_info_initthd_init(rci, sl__globals()->sched_thd, 0);
		}
	} while (remaining > 0);

	for (i = 0; i < (int)MAX_NUM_COMP_WORDS; i++) PRINTLOG(PRINT_DEBUG, "Scheduler bitmap[%d]: %u\n", i, cap_info_schedbmp[cos_cpuid()][i]);
	assert(num_comps == hypercall_numcomps_get());
}



static void
capmgr_comp_info_iter(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	struct cap_comp_info   *btinfo = cap_info_comp_find(0);
	int remaining = 0, i;
	int num_comps = 0;

	do {
		spdid_t spdid = 0, sched_spdid = 0;
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
		struct cos_aep_info aep;

		memset(&aep, 0, sizeof(struct cos_aep_info));

		remaining = hypercall_comp_info_next(&pgtslot, &captslot, &ccslot, &spdid, &sched_spdid);
		if (remaining < 0) {
			assert(remaining == -1); /* iterator end */
			break;
		}

		num_comps ++;
		if (spdid == 0 || (spdid != cos_spd_id() && cap_info_is_child(btinfo, spdid))) {
			is_sched = (spdid == 0 || cap_info_is_sched_child(btinfo, spdid)) ? 1 : 0;

			ret = hypercall_comp_initaep_get(spdid, is_sched, &aep);
			assert(ret == 0);
		}

		ret = hypercall_comp_frontier_get(spdid, &vasfr, &capfr);
		assert(ret == 0);

		rci = cap_info_comp_init(spdid, captslot, pgtslot, ccslot, capfr, vasfr, sched_spdid);
		assert(rci);

		while ((remain_child = hypercall_comp_child_next(spdid, &childid, &ch_flags)) >= 0) {
			bitmap_set(rci->child_bitmap[cos_cpuid()], childid - 1);
			if (ch_flags & COMP_FLAG_SCHED) {
				bitmap_set(rci->child_sched_bitmap[cos_cpuid()], childid - 1);
				bitmap_set(cap_info_schedbmp[cos_cpuid()], childid - 1);
			}

			if (!remain_child) break;
		}

		if (aep.thd) {
			ithd = sl_thd_init_ext(&aep, NULL);
			assert(ithd);

			cap_info_initthd_init(rci, ithd, 0);
		} else if (cos_spd_id() == spdid) {
			cap_info_initthd_init(rci, sl__globals()->sched_thd, 0);
		}
	} while (remaining > 0);

	for (i = 0; i < (int)MAX_NUM_COMP_WORDS; i++) PRINTLOG(PRINT_DEBUG, "Scheduler bitmap[%d]: %u\n", i, cap_info_schedbmp[cos_cpuid()][i]);
	assert(num_comps == hypercall_numcomps_get());
	capmgr_init_core_done = 1;
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	capid_t cap_frontier = 0;
	vaddr_t heap_frontier = 0;
	spdid_t child;
	static int core = 1;
	comp_flag_t ch_flags;
	int ret = 0, i;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));
	ret = hypercall_comp_frontier_get(cos_spd_id(), &heap_frontier, &cap_frontier);
	assert(ret == 0);

	if (cos_cpuid() == INIT_CORE) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE,
				BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
				BOOT_CAPTBL_SELF_COMP, heap_frontier, cap_frontier);
		cap_info_init();
		sl_init(SL_MIN_PERIOD_US);
		capmgr_comp_info_iter();
	} else {
		while (!capmgr_init_core_done) ; /* WAIT FOR INIT CORE TO BE DONE */
		while (core != cos_cpuid()) ; /* continue only if it's this CORE's turn */

		cos_defcompinfo_sched_init();
		sl_init(SL_MIN_PERIOD_US);
		capmgr_comp_info_iter_cpu();
		ps_faa((unsigned long *)&core, 1);
	}
	assert(hypercall_comp_child_next(cos_spd_id(), &child, &ch_flags) == -1);

	PRINTLOG(PRINT_DEBUG, "Initialized CAPABILITY MANAGER\n");

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
