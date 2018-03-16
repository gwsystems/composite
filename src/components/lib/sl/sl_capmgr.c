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
#include "../../interface/capmgr/capmgr.h"

extern struct sl_global sl_global_data;
extern void sl_thd_event_info_reset(struct sl_thd *t);
extern void sl_thd_free_no_cs(struct sl_thd *t);

struct sl_thd *
sl_thd_alloc_init(struct cos_aep_info *aep, asndcap_t sndcap, sl_thd_property_t prps)
{
	struct sl_thd_policy *tp = NULL;
	struct sl_thd        *t  = NULL;

	tp = sl_thd_alloc_backend(aep->tid);
	if (!tp) goto done;
	t  = sl_mod_thd_get(tp);

	t->properties     = prps;
	t->aepinfo        = aep;
	t->sndcap         = sndcap;
	t->state          = SL_THD_RUNNABLE;
	sl_thd_index_add_backend(sl_mod_thd_policy_get(t));

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

static struct sl_thd *
sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	thdcap_t thdcap = 0;
	thdid_t tid = 0;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = capmgr_thd_create(fn, data, &tid);
	if (!aep->thd) goto done;
	aep->tid = tid;

	t = sl_thd_alloc_init(aep, 0, 0);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_comp_init_no_cs(struct cos_defcompinfo *comp, sl_thd_property_t prps, asndcap_t snd)
{
	struct cos_aep_info *sa  = cos_sched_aep_get(comp);
	struct cos_aep_info *aep = NULL;
	struct sl_thd       *t   = NULL;

	if (comp == NULL || comp->id == 0) goto done;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	/* copying cos_aep_info is fine here as cos_thd_alloc() is not done using this aep */
	*aep = *sa;
	if (!snd && (prps & SL_THD_PROPERTY_SEND)) {
		snd = capmgr_asnd_create(comp->id, aep->tid);
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

	if (comp == NULL || comp->id == 0) goto done;

	if (idx) {
		aep = sl_thd_alloc_aep_backend();
		if (!aep) goto done;

		aep->thd = capmgr_thd_create_ext(comp->id, idx, &aep->tid);
		if (!aep->thd) goto done;
		aep->tc  = sl_thd_tcap(sl__globals()->sched_thd);

		t = sl_thd_alloc_init(aep, 0, 0);
		sl_mod_thd_create(sl_mod_thd_policy_get(t));
	} else {
		struct cos_aep_info *compaep = cos_sched_aep_get(comp);

		assert(idx == 0);
		memset(compaep, 0, sizeof(struct cos_aep_info));

		compaep->thd = capmgr_initthd_create(comp->id, &compaep->tid);
		if (!compaep->thd) goto done;

		t = sl_thd_comp_init_no_cs(comp, 0, 0);
	}

done:
	return t;
}

static struct sl_thd *
sl_thd_aep_alloc_ext_no_cs(struct cos_defcompinfo *comp, struct sl_thd *sched, thdclosure_index_t idx, sl_thd_property_t prps, arcvcap_t *extrcv)
{
	struct cos_aep_info *aep = NULL;
	struct sl_thd       *t   = NULL;
	asndcap_t            snd = 0;
	int                  ret = 0, owntc = 0;

	if (comp == NULL || comp->id == 0) goto done;

	if (prps & SL_THD_PROPERTY_SEND) {
		thdcap_t thd;

		aep = cos_sched_aep_get(comp);

		if (prps & SL_THD_PROPERTY_OWN_TCAP) owntc = 1;
		thd = capmgr_initaep_create(comp->id, aep, owntc, &snd);
		if (!thd) goto done;

		t = sl_thd_comp_init_no_cs(comp, prps, snd);
	} else {
		assert(idx > 0);
		aep = sl_thd_alloc_aep_backend();
		if (!aep) goto done;

		if (prps & SL_THD_PROPERTY_OWN_TCAP) owntc = 1;
		capmgr_aep_create_ext(comp->id, aep, idx, owntc, extrcv);
		if (!aep->thd) goto done;

		t = sl_thd_alloc_init(aep, 0, prps);
		sl_mod_thd_create(sl_mod_thd_policy_get(t));
	}

done:
	return t;
}

static struct sl_thd *
sl_thd_aep_alloc_no_cs(cos_aepthd_fn_t fn, void *data, struct cos_defcompinfo *comp, sl_thd_property_t prps)
{
	struct sl_thd       *t     = NULL;
	struct cos_aep_info *aep   = NULL;
	int                  owntc = 0;

	if (comp == NULL || comp->id == 0) goto done;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	if (prps & SL_THD_PROPERTY_OWN_TCAP) owntc = 1;
	capmgr_aep_create(aep, fn, data, owntc);
	if (aep->thd == 0) goto done;

	t = sl_thd_alloc_init(aep, 0, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

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
sl_thd_aep_alloc(cos_aepthd_fn_t fn, void *data, int own_tcap)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_aep_alloc_no_cs(fn, data, NULL, own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0);
	sl_cs_exit();

	return t;
}

/* sl object for inithd in the child comp */
struct sl_thd *
sl_thd_comp_init(struct cos_defcompinfo *comp, int is_sched)
{
	struct sl_thd *t = NULL;

	if (comp == NULL || comp->id == 0) return NULL;

	sl_cs_enter();
	t = sl_thd_comp_init_no_cs(comp, is_sched ? SL_THD_PROPERTY_OWN_TCAP | SL_THD_PROPERTY_SEND : 0, 0);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_initaep_alloc(struct cos_defcompinfo *comp, struct sl_thd *sched_thd, int is_sched, int own_tcap)
{
	struct sl_thd *t = NULL;

	if (comp == NULL || comp->id == 0) return NULL;

	sl_cs_enter();
	if (!is_sched) {
		t = sl_thd_alloc_ext_no_cs(comp, 0);
	} else {
		t = sl_thd_aep_alloc_ext_no_cs(comp, sched_thd, 0, (is_sched ? SL_THD_PROPERTY_SEND : 0)
					       | (own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0), NULL);
	}
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_aep_alloc_ext(struct cos_defcompinfo *comp, struct sl_thd *sched_thd, thdclosure_index_t idx, int is_aep, int own_tcap, arcvcap_t *extrcv)
{
	struct sl_thd *t = NULL;

	if (comp == NULL || comp->id == 0) return NULL;
	if (idx <= 0) return NULL;

	sl_cs_enter();
	if (!is_aep) own_tcap = 0;
	if (is_aep) {
		t = sl_thd_aep_alloc_ext_no_cs(comp, sched_thd, idx, own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0, extrcv);
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

void
sl_thd_free(struct sl_thd *t)
{
	assert(t);

	sl_cs_enter();
	sl_thd_free_no_cs(t);
	sl_cs_exit();
}
