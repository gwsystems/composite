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

extern struct sl_global sl_global_data;
extern void sl_thd_event_info_reset(struct sl_thd *t);
extern void sl_thd_free_intern(struct sl_thd *t);

struct sl_thd *
sl_thd_alloc_init(thdid_t tid, struct cos_aep_info *aep, asndcap_t sndcap, sl_thd_property_t prps)
{
	struct sl_thd_policy *tp = NULL;
	struct sl_thd        *t  = NULL;

	tp = sl_thd_alloc_backend(tid);
	if (!tp) goto done;
	t  = sl_mod_thd_get(tp);

	t->thdid          = tid;
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
sl_thd_ext_idx_alloc_intern(struct cos_defcompinfo *comp, int idx)
{
	struct cos_defcompinfo *dci   = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(dci);
	struct cos_compinfo    *dstci = cos_compinfo_get(comp);
	struct sl_thd          *t     = NULL;
	struct cos_aep_info    *aep   = NULL;
	thdcap_t thdcap;
	thdid_t tid;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = cos_thd_alloc_idx(ci, dstci->comp_cap, idx);
	if (!aep->thd) goto done;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, 0, 0);
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
	thdcap_t thdcap;
	thdid_t tid;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = cos_thd_alloc(ci, ci->comp_cap, fn, data);
	if (!aep->thd) goto done;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, 0, 0);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_extaep_idx_alloc_intern(struct cos_defcompinfo *comp, struct sl_thd *schthd, int idx, sl_thd_property_t prps, arcvcap_t *extrcv)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	thdid_t                 tid;
	int                     ret;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	if (prps & SL_THD_PROPERTY_OWN_TCAP) ret = cos_aep_idx_alloc_ext(aep, idx, comp, sl_thd_aepinfo(schthd));
	else                                 ret = cos_aep_idx_tcap_alloc_ext(aep, sl_thd_tcap(schthd), idx, comp, sl_thd_aepinfo(schthd));
	if (ret) goto done;

	/* FIXME: copy rcvcap to comp and return extrcv */
	*extrcv = 0;
	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, 0, SL_THD_PROPERTY_OWN_TCAP);
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
	thdid_t tid;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	aep->thd = thd;
	aep->tc  = tc;
	aep->rcv = rcv;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, snd, prps);
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
	thdid_t                 tid;
	int                     ret;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;

	if (prps & SL_THD_PROPERTY_SEND) {
		struct cos_aep_info *sa = NULL;

		assert(comp);
		sa   = cos_sched_aep_get(comp);
		/* copying cos_aep_info is fine here as cos_thd_alloc() is not done using this aep */
		*aep = *sa;

		snd = cos_asnd_alloc(ci, aep->rcv, ci->captbl_cap);
		assert(snd);
	} else {
		/* IMP: Cannot use stack-allocated cos_aep_info struct here */
		if (prps & SL_THD_PROPERTY_OWN_TCAP) ret = cos_aep_alloc(aep, fn, data);
		else                                 ret = cos_aep_tcap_alloc(aep, sl_thd_aepinfo(sl__globals()->sched_thd)->tc,
									      fn, data);
		if (ret) goto done;
	}

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, snd, prps);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_ext_childaep_alloc_intern(struct cos_defcompinfo *comp, struct sl_thd *sched, sl_thd_property_t prps)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	asndcap_t               snd = 0;
	struct cos_aep_info    *aep = NULL;
	struct cos_aep_info    *sa  = NULL;
	thdid_t                 tid;
	int                     ret;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;
	sa  = cos_sched_aep_get(comp);

	assert(prps & SL_THD_PROPERTY_SEND);

	if (prps & SL_THD_PROPERTY_OWN_TCAP) ret = cos_child_initaep_alloc_ext(comp, sl_thd_aepinfo(sched));
	else                                 ret = cos_child_initaep_tcap_alloc_ext(comp, sl_thd_tcap(sched), sl_thd_aepinfo(sched)); 
	if (ret) goto done;

	snd = cos_asnd_alloc(ci, sa->rcv, ci->captbl_cap);
	assert(snd);
	*aep = *sa;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, snd, prps);
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
	thdid_t                 tid;
	int                     ret;

	aep = sl_thd_alloc_aep_backend();
	if (!aep) goto done;
	sa  = cos_sched_aep_get(comp);

	if (prps & SL_THD_PROPERTY_SEND) {
		if (prps & SL_THD_PROPERTY_OWN_TCAP) ret = cos_child_initaep_alloc(comp);
		else                                 ret = cos_child_initaep_tcap_alloc(comp, sl_thd_tcap(g->sched_thd)); 
		snd = cos_asnd_alloc(ci, sa->rcv, ci->captbl_cap);
		assert(snd);
	} else {
		ret = cos_child_initthd_alloc(comp);
		if (ret) goto done;
	}
	*aep = *sa;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, snd, prps);
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
	thdid_t tid;

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
		tid = cos_introspect(ci, aep->thd, THD_GET_TID);
		assert(tid);
		t   = sl_thd_alloc_init(tid, aep, 0, 0);
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
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	thdid_t tid;

	assert(comp);

	sl_cs_enter();
	t = sl_thd_ext_childaep_alloc_intern(comp, sched, SL_THD_PROPERTY_SEND
					| (own_tcap ? SL_THD_PROPERTY_OWN_TCAP : 0));
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_ext_idx_alloc(struct cos_defcompinfo *comp, int idx)
{
	struct sl_thd *t = NULL;

	sl_cs_enter();
	t = sl_thd_ext_idx_alloc_intern(comp, idx);;
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_extaep_idx_alloc(struct cos_defcompinfo *comp, struct sl_thd *sched, int idx, int own_tcap, arcvcap_t *extrcv)
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
