/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <ps.h>
#include <sl.h>
#include <sl_mod_policy.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>
#include <bitmap.h>

extern void sl_thd_event_info_reset(struct sl_thd *t);
extern void sl_thd_free_no_cs(struct sl_thd *t);

cbuf_t
sl_shm_alloc(vaddr_t *addr)
{
	return 0;
}

vaddr_t
sl_shm_map(cbuf_t id)
{
        return 0;
}

void
sl_xcpu_asnd_alloc(void)
{
        struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
        struct cos_compinfo    *ci  = cos_compinfo_get(dci);
	int i;

	for (i = 0; i < NUM_CPU; i++) {
		asndcap_t snd;

		if (i == cos_cpuid()) continue;
		if (!bitmap_check(sl__globals()->cpu_bmp, i)) continue;

		snd = cos_asnd_alloc(ci, BOOT_CAPTBL_SELF_INITRCV_BASE_CPU(i), ci->captbl_cap);
		assert(snd);
		sl__globals()->xcpu_asnd[cos_cpuid()][i] = snd;
	}
}

struct sl_thd *
sl_thd_alloc_init(struct cos_aep_info *aep, asndcap_t sndcap, sl_thd_property_t prps)
{
	struct sl_thd_policy *tp = NULL;
	struct sl_thd        *t  = NULL;

	assert(aep->tid);
	tp = sl_thd_alloc_backend(aep->tid);
	if (!tp) goto done;
	t  = sl_mod_thd_get(tp);

	t->properties     = prps;
	t->aepinfo        = aep;
	t->sndcap         = sndcap;
	t->state          = SL_THD_RUNNABLE;
	sl_thd_index_add_backend(sl_mod_thd_policy_get(t));

	t->rcv_suspended  = 0;
	t->budget         = 0;
	t->last_replenish = 0;
	t->period         = t->timeout_cycs = t->periodic_cycs = 0;
	t->wakeup_cycs    = 0;
	t->timeout_idx    = -1;
	t->prio           = TCAP_PRIO_MIN;
	ps_list_init(t, SL_THD_EVENT_LIST);
	sl_thd_event_info_reset(t);

done:
	return t;
}

struct sl_thd *
sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = cos_compinfo_get(dci);
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = cos_thd_alloc(ci, ci->comp_cap, fn, data);
	if (!aep->thd) goto done;
	aep->tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	if (!aep->tid) goto done;

	t = sl_thd_alloc_init(aep, 0, 0);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_comp_init_no_cs(struct cos_defcompinfo *comp, sl_thd_property_t prps, asndcap_t snd)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = cos_compinfo_get(dci);
	struct cos_aep_info    *sa  = cos_sched_aep_get(comp);
	struct cos_aep_info    *aep = NULL;
	struct sl_thd          *t   = NULL;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	/* copying cos_aep_info is fine here as cos_thd_alloc() is not done using this aep */
	*aep = *sa;
	if (!snd && (prps & SL_THD_PROPERTY_SEND)) {
		snd = cos_asnd_alloc(ci, aep->rcv, ci->captbl_cap);
		assert(snd);
	}

	t = sl_thd_alloc_init(aep, snd, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_alloc_ext_no_cs(struct cos_defcompinfo *comp, thdclosure_index_t idx)
{
	struct cos_defcompinfo *dci    = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci     = cos_compinfo_get(dci);
	struct cos_compinfo    *compci = cos_compinfo_get(comp);
	struct sl_thd          *t      = NULL;
	struct cos_aep_info    *aep    = NULL;
	int                     ret;

	if (idx) {
		aep = sl_thd_alloc_aep_backend();
		if (!aep) goto done;

		aep->thd = cos_thd_alloc_ext(ci, compci->comp_cap, idx);
		if (!aep->thd) goto done;
		aep->tid = cos_introspect(ci, aep->thd, THD_GET_TID);
		if (!aep->tid) goto done;

		t = sl_thd_alloc_init(aep, 0, 0);
		sl_mod_thd_create(sl_mod_thd_policy_get(t));
	} else {
		assert(idx == 0);
		ret = cos_initaep_alloc(comp, NULL, 0);
		if (ret) goto done;

		t = sl_thd_comp_init_no_cs(comp, 0, 0);
	}

done:
	return t;
}

static struct sl_thd *
sl_thd_aep_alloc_no_cs(cos_aepthd_fn_t fn, void *data, sl_thd_property_t prps, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	int                     ret;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	/* NOTE: Cannot use stack-allocated cos_aep_info struct here */
	if (prps & SL_THD_PROPERTY_OWN_TCAP) ret = cos_aep_alloc(aep, fn, data);
	else                                 ret = cos_aep_tcap_alloc(aep, sl_thd_aepinfo(sl__globals_cpu()->sched_thd)->tc,
			                                              fn, data);
	if (ret) goto done;

	t = sl_thd_alloc_init(aep, 0, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_aep_alloc_ext_no_cs(struct cos_defcompinfo *comp, struct sl_thd *sched, thdclosure_index_t idx, sl_thd_property_t prps, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv)
{
	struct cos_aep_info *aep = NULL;
	struct sl_thd       *t   = NULL;
	asndcap_t            snd = 0;
	int                  ret = 0;

	if (prps & SL_THD_PROPERTY_SEND) {
		assert(sched);
		if (prps & SL_THD_PROPERTY_OWN_TCAP) {
			ret = cos_initaep_alloc(comp, sl_thd_aepinfo(sched), prps & SL_THD_PROPERTY_SEND);
		} else {
			ret = cos_initaep_tcap_alloc(comp, sl_thd_tcap(sched), sl_thd_aepinfo(sched));
		}
		if (ret) goto done;

		t = sl_thd_comp_init_no_cs(comp, prps, 0);
	} else {
		assert(idx > 0);
		assert(sched);
		aep = sl_thd_alloc_aep_backend();
		if (!aep) goto done;

		if (prps & SL_THD_PROPERTY_OWN_TCAP) {
			ret = cos_aep_alloc_ext(aep, comp, sl_thd_aepinfo(sched), idx);
		} else {
			ret = cos_aep_tcap_alloc_ext(aep, comp, sl_thd_aepinfo(sched), sl_thd_tcap(sched), idx);
		}
		if (ret) goto done;

		t = sl_thd_alloc_init(aep, 0, prps);
		sl_mod_thd_create(sl_mod_thd_policy_get(t));

		if (extrcv) *extrcv = sl_thd_rcvcap(t);
	}

done:
	return t;
}

struct sl_thd *
sl_thd_alloc(cos_thd_fn_t fn, void *data)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_alloc_no_cs(fn, data);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_aep_alloc(cos_aepthd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_aep_alloc_no_cs(fn, data, own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0, 0, ipiwin, ipimax);
	sl_cs_exit();

	return t;
}

/* sl object for inithd in the child comp */
struct sl_thd *
sl_thd_comp_init(struct cos_defcompinfo *comp, int is_sched)
{
	struct sl_thd *t = NULL;

	if (!comp) return NULL;

	sl_cs_enter();
	t = sl_thd_comp_init_no_cs(comp, is_sched ? SL_THD_PROPERTY_OWN_TCAP | SL_THD_PROPERTY_SEND : 0, 0);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_initaep_alloc(struct cos_defcompinfo *comp, struct sl_thd *sched_thd, int is_sched, int own_tcap, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	struct sl_thd *t = NULL;

	if (!comp) return NULL;

	sl_cs_enter();
	if (!is_sched) t = sl_thd_alloc_ext_no_cs(comp, 0);
	else           t = sl_thd_aep_alloc_ext_no_cs(comp, sched_thd, 0, (is_sched ? SL_THD_PROPERTY_SEND : 0)
						      | (own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0), key, ipiwin, ipimax, NULL);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_aep_alloc_ext(struct cos_defcompinfo *comp, struct sl_thd *sched_thd, thdclosure_index_t idx, int is_aep, int own_tcap, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv)
{
	struct sl_thd *t = NULL;

	if (!comp || idx <= 0) return NULL;
	sl_cs_enter();
	if (!is_aep) own_tcap = 0;
	if (is_aep) {
		t = sl_thd_aep_alloc_ext_no_cs(comp, sched_thd, idx, own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0, key, ipiwin, ipimax, extrcv);
	} else {
		t = sl_thd_alloc_ext_no_cs(comp, idx);
	}
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_init_ext(struct cos_aep_info *aepthd, struct sl_thd *sched)
{
	struct sl_thd       *t   = NULL;
	struct cos_aep_info *aep = NULL;

	if (!aepthd || !aepthd->thd || !aepthd->tid) return NULL;

	sl_cs_enter();
	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	*aep = *aepthd;
	/* TODO: use sched info for parent -> child notifications */
	t = sl_thd_alloc_init(aep, 0, 0);

done:
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_retrieve(thdid_t tid)
{
	return sl_mod_thd_get(sl_thd_lookup_backend(tid));
}

void
sl_thd_free(struct sl_thd *t)
{
	assert(t);

	sl_cs_enter();
	sl_thd_free_no_cs(t);
	sl_cs_exit();
}
