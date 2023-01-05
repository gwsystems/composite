/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_defkernel_api.h>
#include <initargs.h>

enum cos_defcompinfo_status
{
	UNINITIALIZED = 0,
	INITIALIZED,
};

static int                    curr_defci_init_status;
static struct cos_defcompinfo curr_defci;

struct cos_defcompinfo *
cos_defcompinfo_curr_get(void)
{
	return &curr_defci;
}

struct cos_compinfo *
cos_compinfo_get(struct cos_defcompinfo *defci)
{
	assert(defci);
	return &(defci->ci);
}

struct cos_aep_info *
cos_sched_aep_get(struct cos_defcompinfo *defci)
{
	assert(defci);
	return &(defci->sched_aep[cos_cpuid()]);
}

void
cos_defcompinfo_init(void)
{
	capid_t cap_frontier = atol(args_get("captbl_end"));

	assert(cap_frontier > 0);
	if (curr_defci_init_status == INITIALIZED) return;

	cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE,
	                         BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
	                         BOOT_CAPTBL_SELF_COMP, (vaddr_t)cos_get_heap_ptr(), cap_frontier);

}

void
cos_defcompinfo_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv, pgtblcap_t pgtbl_cap,
                         captblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	if (curr_defci_init_status == INITIALIZED) return;

	cos_compinfo_init(ci, pgtbl_cap, captbl_cap, comp_cap, heap_ptr, cap_frontier, ci);
	curr_defci_init_status = INITIALIZED;
	cos_defcompinfo_sched_init_ext(sched_tc, sched_thd, sched_rcv);
}

void
cos_defcompinfo_sched_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci        = cos_compinfo_get(defci);
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);

	assert(curr_defci_init_status == INITIALIZED);

	sched_aep->tc   = sched_tc;
	sched_aep->thd  = sched_thd;
	sched_aep->rcv  = sched_rcv;
	sched_aep->fn   = NULL;
	sched_aep->data = NULL;
	sched_aep->tid  = cos_introspect(ci, sched_thd, THD_GET_TID);
}

void
cos_defcompinfo_sched_init(void)
{
	assert(curr_defci_init_status == INITIALIZED);

	cos_defcompinfo_sched_init_ext(BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE,
				       BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
}

static int
cos_aep_alloc_intern(struct cos_aep_info *aep, struct cos_defcompinfo *dst_dci, tcap_t tc, struct cos_aep_info *sched, cos_aepthd_fn_t fn, void *data, thdclosure_index_t idx)
{
	struct cos_defcompinfo *defci   = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci      = cos_compinfo_get(defci);
	struct cos_compinfo    *dst_ci  = cos_compinfo_get(dst_dci);
	int                     is_init = (!fn && !data && !idx);

	assert(curr_defci_init_status == INITIALIZED);
	memset(aep, 0, sizeof(struct cos_aep_info));

	if (is_init)      aep->thd = cos_initthd_alloc(ci, dst_ci->comp_cap);
	else if (idx > 0) aep->thd = cos_thd_alloc_ext(ci, dst_ci->comp_cap, idx);
	else              aep->thd = cos_thd_alloc(ci, dst_ci->comp_cap, cos_aepthd_fn, (void *)aep);
	assert(aep->thd);
	aep->tid  = cos_introspect(ci, aep->thd, THD_GET_TID);
	if (!sched && is_init) return 0;

	if (tc) {
		aep->tc = tc;
	} else {
		aep->tc = cos_tcap_alloc(ci);
		assert(aep->tc);
	}

	aep->rcv  = cos_arcv_alloc(ci, aep->thd, aep->tc, ci->comp_cap, sched->rcv);
	assert(aep->rcv);
	aep->fn   = fn;
	aep->data = data;

	return 0;
}

int
cos_defcompinfo_child_alloc(struct cos_defcompinfo *child_defci, vaddr_t entry, vaddr_t heap_ptr, capid_t cap_frontier,
                            int is_sched)
{
	int                     ret;
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);
	struct cos_compinfo    *ci        = cos_compinfo_get(defci);
	struct cos_compinfo    *child_ci  = cos_compinfo_get(child_defci);
	struct cos_aep_info    *child_aep = cos_sched_aep_get(child_defci);

	assert(curr_defci_init_status == INITIALIZED);
	ret = cos_compinfo_alloc(child_ci, heap_ptr, cap_frontier, entry, ci, 0);
	if (ret) return ret;
	ret = cos_aep_alloc_intern(child_aep, child_defci, 0, is_sched ? sched_aep : NULL, NULL, NULL, 0);

	return ret;
}

void
cos_defcompinfo_childid_init(struct cos_defcompinfo *child_defci, spdid_t c)
{
	assert(child_defci != cos_defcompinfo_curr_get());

	child_defci->id = c;
}

int
cos_initaep_alloc(struct cos_defcompinfo *dst_dci, struct cos_aep_info *sched, int is_sched)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);
	struct cos_aep_info    *child_aep = cos_sched_aep_get(dst_dci);
	struct cos_aep_info    *sched_use = is_sched ? (sched ? sched : sched_aep) : NULL;

	return cos_aep_alloc_intern(child_aep, dst_dci, 0, sched_use, NULL, NULL, 0);
}

int
cos_initaep_tcap_alloc(struct cos_defcompinfo *dst_dci, tcap_t tc, struct cos_aep_info *sched)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);
	struct cos_aep_info    *child_aep = cos_sched_aep_get(dst_dci);
	struct cos_aep_info    *sched_use = sched ? sched : sched_aep;

	return cos_aep_alloc_intern(child_aep, dst_dci, tc, sched_use, NULL, NULL, 0);
}

int
cos_aep_alloc_ext(struct cos_aep_info *aep, struct cos_defcompinfo *dst_dci, struct cos_aep_info *sched, thdclosure_index_t idx)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);

	assert(aep && idx > 0);
	if (!sched) sched_aep = cos_sched_aep_get(dst_dci);
	else        sched_aep = sched;

	return cos_aep_alloc_intern(aep, dst_dci, 0, sched_aep, NULL, NULL, idx);
}

int
cos_aep_tcap_alloc_ext(struct cos_aep_info *aep, struct cos_defcompinfo *dst_dci, struct cos_aep_info *sched, tcap_t tc, thdclosure_index_t idx)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);

	assert(aep);
	assert(idx > 0);
	if (!sched) sched_aep = cos_sched_aep_get(dst_dci);
	else        sched_aep = sched;

	return cos_aep_alloc_intern(aep, dst_dci, tc, sched_aep, NULL, NULL, idx);
}

int
cos_aep_alloc(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);

	return cos_aep_alloc_intern(aep, defci, 0, sched_aep, fn, data, 0);
}

int
cos_aep_tcap_alloc(struct cos_aep_info *aep, tcap_t tc, cos_aepthd_fn_t fn, void *data)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);

	return cos_aep_alloc_intern(aep, defci, tc, sched_aep, fn, data, 0);
}

int
cos_defswitch(thdcap_t c, tcap_prio_t p, tcap_time_t r, sched_tok_t stok)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = cos_sched_aep_get(defci);

	assert(curr_defci_init_status == INITIALIZED);

	return cos_switch(c, sched_aep->tc, p, r, sched_aep->rcv, stok);
}
