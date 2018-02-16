/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_defkernel_api.h>

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
	return &(defci->sched_aep);
}

void
cos_defcompinfo_init(void)
{
	if (curr_defci_init_status == INITIALIZED) return;

	cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_BASE, BOOT_CAPTBL_SELF_INITTHD_BASE,
	                         BOOT_CAPTBL_SELF_INITRCV_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
	                         BOOT_CAPTBL_SELF_COMP, (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE);

	curr_defci_init_status = INITIALIZED;
}

void
cos_defcompinfo_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv, pgtblcap_t pgtbl_cap,
                         captblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci        = cos_compinfo_get(defci);
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);

	if (curr_defci_init_status == INITIALIZED) return;

	sched_aep->tc   = sched_tc;
	sched_aep->thd  = sched_thd;
	sched_aep->rcv  = sched_rcv;
	sched_aep->fn   = NULL;
	sched_aep->data = NULL;

	cos_compinfo_init(ci, pgtbl_cap, captbl_cap, comp_cap, heap_ptr, cap_frontier, ci);
	curr_defci_init_status = INITIALIZED;
}

int
cos_defcompinfo_child_alloc(struct cos_defcompinfo *child_defci, vaddr_t entry, vaddr_t heap_ptr, capid_t cap_frontier,
                            int is_sched)
{
	int                     ret;
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);
	struct cos_compinfo *   ci        = cos_compinfo_get(defci);
	struct cos_aep_info *   child_aep = cos_sched_aep_get(child_defci);
	struct cos_compinfo *   child_ci  = cos_compinfo_get(child_defci);

	assert(curr_defci_init_status == INITIALIZED);
	ret = cos_compinfo_alloc(child_ci, heap_ptr, cap_frontier, entry, ci);
	if (ret) return ret;

	child_aep->thd = cos_initthd_alloc(ci, child_ci->comp_cap);
	assert(child_aep->thd);

	if (is_sched) {
		child_aep->tc = cos_tcap_alloc(ci);
		assert(child_aep->tc);

		child_aep->rcv = cos_arcv_alloc(ci, child_aep->thd, child_aep->tc, ci->comp_cap, sched_aep->rcv);
		assert(child_aep->rcv);
	} else {
		child_aep->tc  = sched_aep->tc;
		child_aep->rcv = sched_aep->rcv;
	}

	child_aep->fn   = NULL;
	child_aep->data = NULL;

	return ret;
}

static void
__aepthd_fn(void *data)
{
	struct cos_aep_info *aep_info = (struct cos_aep_info *)data;
	cos_aepthd_fn_t      aep_fn   = aep_info->fn;
	void *               fn_data  = aep_info->data;

	(aep_fn)(aep_info->rcv, fn_data);
}

int
cos_aep_alloc(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	assert(curr_defci_init_status == INITIALIZED);
	tcap_t tc = cos_tcap_alloc(ci);
	assert(tc);

	return cos_aep_tcap_alloc(aep, tc, fn, data);
}

int
cos_aep_tcap_alloc(struct cos_aep_info *aep, tcap_t tc, cos_aepthd_fn_t fn, void *data)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);
	struct cos_compinfo *   ci        = cos_compinfo_get(defci);

	assert(curr_defci_init_status == INITIALIZED);
	memset(aep, 0, sizeof(struct cos_aep_info));

	aep->thd = cos_thd_alloc(ci, ci->comp_cap, __aepthd_fn, (void *)aep);
	assert(aep->thd);

	aep->tc  = tc;
	aep->rcv = cos_arcv_alloc(ci, aep->thd, aep->tc, ci->comp_cap, sched_aep->rcv);
	assert(aep->rcv);

	aep->fn   = fn;
	aep->data = data;

	return 0;
}

int
cos_defswitch(thdcap_t c, tcap_prio_t p, tcap_time_t r, sched_tok_t stok)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);

	assert(curr_defci_init_status == INITIALIZED);

	return cos_switch(c, sched_aep->tc, p, r, sched_aep->rcv, stok);
}
