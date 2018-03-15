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
extern void sl_thd_free_intern(struct sl_thd *t);

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
sl_thd_ext_idx_alloc_intern(struct cos_defcompinfo *comp, thdclosure_index_t idx)
{
	struct cos_defcompinfo *dci   = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(dci);
	struct sl_thd          *t     = NULL;
	struct cos_aep_info    *aep   = NULL;
	thdcap_t thdcap = 0;
	thdid_t tid = 0;

	if (comp == NULL || comp->id == 0) goto done;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = capmgr_ext_thd_create(comp->id, idx, &tid);
	if (!aep->thd) goto done;
	aep->tid = tid;

	t = sl_thd_alloc_init(aep, 0, 0);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_alloc_intern(cos_thd_fn_t fn, void *data)
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
sl_thd_extaep_idx_alloc_intern(struct cos_defcompinfo *comp, struct sl_thd *schthd, thdclosure_index_t idx, sl_thd_property_t prps, arcvcap_t *extrcv)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	int                     ret;
	int                     owntc = 0;

	if (comp == NULL || comp->id == 0) goto done;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	if (prps & SL_THD_PROPERTY_OWN_TCAP) owntc = 1;
	ret = capmgr_ext_aep_create(comp->id, aep, idx, owntc, extrcv);
	if (!ret) goto done;

	t = sl_thd_alloc_init(aep, 0, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_ext_init_intern(thdcap_t thd, tcap_t tc, arcvcap_t rcv, asndcap_t snd, sl_thd_property_t prps)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct cos_aep_info    *aep = NULL;
	struct sl_thd          *t   = NULL;

	if (!thd) goto done;
	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = thd;
	aep->tc  = tc;
	aep->rcv = rcv;
	aep->tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	if (!aep->tid) goto done;

	t = sl_thd_alloc_init(aep, snd, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_aep_alloc_intern(cos_aepthd_fn_t fn, void *data, struct cos_defcompinfo *comp, sl_thd_property_t prps)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	asndcap_t               snd = 0;
	struct cos_aep_info    *aep = NULL;
	int                     ret;
	int                     owntc = 0;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	if (prps & SL_THD_PROPERTY_SEND) {
		struct cos_aep_info *sa = NULL;

		if (comp == NULL || comp->id == 0) goto done;

		sa   = cos_sched_aep_get(comp);
		/* copying cos_aep_info is fine here as cos_thd_alloc() is not done using this aep */
		*aep = *sa;
	} else {
		if (prps & SL_THD_PROPERTY_OWN_TCAP) owntc = 1;

		capmgr_aep_create(aep, fn, data, owntc);
	}
	if (aep->thd == 0) goto done;

	if (prps & SL_THD_PROPERTY_OWN_TCAP && snd == 0) {
		snd = capmgr_asnd_create(comp->id, aep->tid);
		assert(snd);
	}
	t = sl_thd_alloc_init(aep, snd, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_childaep_alloc_intern(struct cos_defcompinfo *comp, sl_thd_property_t prps)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_global       *g   = sl__globals();
	struct sl_thd          *t   = NULL;
	asndcap_t               snd = 0;
	struct cos_aep_info    *aep = NULL;
	struct cos_aep_info    *sa  = NULL;
	thdid_t                 tid = 0;
	int                     ret;
	int                     owntc = 0;

	if (comp == NULL || comp->id == 0) goto done;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;
	sa  = cos_sched_aep_get(comp);

	if (prps & SL_THD_PROPERTY_SEND) {
		if (prps & SL_THD_PROPERTY_OWN_TCAP) owntc = 1;
		capmgr_initaep_create(comp->id, aep, owntc, &snd);
	} else {
		aep->thd = capmgr_initthd_create(comp->id, &tid);
		aep->tid = tid;
	}
	if (aep->thd == 0) goto done;
	*sa = *aep;

	t = sl_thd_alloc_init(aep, snd, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

struct sl_thd *
sl_thd_alloc(cos_thd_fn_t fn, void *data)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_alloc_intern(fn, data);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_aep_alloc(cos_aepthd_fn_t fn, void *data, int own_tcap)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_aep_alloc_intern(fn, data, NULL, own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0);
	sl_cs_exit();

	return t;
}

/* sl object for inithd in the child comp */
struct sl_thd *
sl_thd_comp_init(struct cos_defcompinfo *comp, int is_sched)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;

	assert(comp);

	sl_cs_enter();
	if (is_sched) {
		t = sl_thd_aep_alloc_intern(NULL, NULL, comp, SL_THD_PROPERTY_OWN_TCAP | SL_THD_PROPERTY_SEND);
	} else {
		struct cos_aep_info *sa = cos_sched_aep_get(comp), *aep = NULL;

		aep = sl_thd_alloc_aep_backend();
		if (!aep) goto done;

		/* copying cos_aep_info is fine here as cos_thd_alloc() is not done using this aep */
		*aep = *sa;
		t    = sl_thd_alloc_init(aep, 0, 0);
		sl_mod_thd_create(sl_mod_thd_policy_get(t));
	}

done:
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_child_initaep_alloc(struct cos_defcompinfo *comp, int is_sched, int own_tcap)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	thdid_t tid;

	assert(comp);

	if (!is_sched) own_tcap = 0;

	sl_cs_enter();
	t = sl_thd_childaep_alloc_intern(comp, (is_sched ? SL_THD_PROPERTY_SEND : 0)
					| (own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0));
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_ext_child_initaep_alloc(struct cos_defcompinfo *comp, struct sl_thd *sched, int own_tcap)
{
	assert(0);

	return NULL;
}

struct sl_thd *
sl_thd_ext_idx_alloc(struct cos_defcompinfo *comp, thdclosure_index_t idx)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_ext_idx_alloc_intern(comp, idx);;
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_extaep_idx_alloc(struct cos_defcompinfo *comp, struct sl_thd *sched, thdclosure_index_t idx, int own_tcap, arcvcap_t *extrcv)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_extaep_idx_alloc_intern(comp, sched, idx, own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0, extrcv);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_ext_init(thdcap_t thd, tcap_t tc, arcvcap_t rcv, asndcap_t snd)
{
	struct sl_global *g = sl__globals();
	struct sl_thd    *t = NULL;
	sl_thd_property_t props = 0;

	if (snd) props |= SL_THD_PROPERTY_SEND;
	if (tc != sl_thd_tcap(g->sched_thd)) props |= SL_THD_PROPERTY_OWN_TCAP;

	sl_cs_enter();
	t = sl_thd_ext_init_intern(thd, tc, rcv, snd, props);
	sl_cs_exit();

	return t;
}

void
sl_thd_free(struct sl_thd *t)
{
	struct sl_thd *ct = sl_thd_curr();

	assert(t);

	sl_cs_enter();
	sl_thd_free_intern(t);
	sl_cs_exit();
}
