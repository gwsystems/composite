#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <capmgr.h>
#include <cap_info.h>
#include <hypercall.h>
#include <sl.h>

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
		struct cos_compinfo tmp;
		struct sl_thd *ithd = NULL;
		u64_t chbits = 0, chschbits = 0;
		pgtblcap_t pgtslot = 0;
		captblcap_t captslot = 0;
		compcap_t ccslot = 0;
		vaddr_t vasfr = 0, src_pg = 0;
		capid_t capfr = 0;
		int ret = 0, is_sched = 0, offset = 0;
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
			bitmap_set(rci->child_bitmap, childid - 1);
			if (ch_flags & COMP_FLAG_SCHED) {
				bitmap_set(rci->child_sched_bitmap, childid - 1);
				bitmap_set(cap_info_schedbmp, childid - 1);
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

		/*
		 * Set tls region for this component
		 * Expand the 2nd level pte within this component's page table at our new range
		 */
		tmp.pgtbl_cap = pgtslot;
		tmp.memsrc    = ci;
		ret = (int)cos_pgtbl_intern_alloc(&tmp, pgtslot, TLS_BASE_ADDR,
					     PAGE_SIZE * TLS_NUM_PAGES);
		assert(ret);

		/* Place desired number of pages at this new range */
		offset = 0;
		for (i = 0 ; i < (signed)TLS_NUM_PAGES ; i++) {
			src_pg = (vaddr_t)cos_page_bump_alloc(ci);
			assert(src_pg);
			ret = cos_mem_alias_at(&tmp, TLS_BASE_ADDR + offset, ci, src_pg);
			assert(!ret);
			offset += PAGE_SIZE;
		}
	} while (remaining > 0);

	for (i = 0; i < (int)MAX_NUM_COMP_WORDS; i++) PRINTLOG(PRINT_DEBUG, "Scheduler bitmap[%d]: %u\n", i, cap_info_schedbmp[i]);
	assert(num_comps == hypercall_numcomps_get());
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

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));
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

	PRINTLOG(PRINT_DEBUG, "Initialized CAPABILITY MANAGER\n");

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
