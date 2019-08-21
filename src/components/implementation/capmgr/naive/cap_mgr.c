/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <capmgr.h>
#include <cap_info.h>

thdcap_t
capmgr_thd_create_cserialized(thdid_t *tid, int *unused, thdclosure_index_t idx)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *r       = cap_info_comp_find(cur);
	struct sl_thd          *rt      = NULL, *t = NULL;
	thdcap_t                thdcap  = 0;

	if (!r || !cap_info_init_check(r)) return 0;
	if (!cap_info_is_sched(cur)) return 0;
	if (idx <= 0) return 0;

	t = sl_thd_aep_alloc_ext(cap_info_dci(r), NULL, idx, 0, 0, 0, 0, 0, NULL);
	if (!t) return 0;
	thdcap = cos_cap_cpy(cap_info_ci(r), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!thdcap) goto err;

	cap_info_thd_init(r, t, 0);
	*tid = sl_thd_thdid(t);

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_thd_create_ext_cserialized(thdid_t *tid, int *unused, spdid_t s, thdclosure_index_t idx)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *t       = NULL;
	thdcap_t                thdcap  = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (cap_info_is_sched(s)) return 0;
	if (idx <= 0) return 0;

	t = sl_thd_aep_alloc_ext(cap_info_dci(rs), NULL, idx, 0, 0, 0, 0, 0, NULL);
	if (!t) return 0;
	thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!thdcap) goto err;

	cap_info_thd_init(rc, t, 0);
	cap_info_thd_init(rs, t, 0);
	*tid = sl_thd_thdid(t);
	/* child is not a scheduler, don't copy into child */

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_initthd_create_cserialized(thdid_t *tid, int *unused, spdid_t s)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *t       = NULL;
	thdcap_t                thdcap  = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (cap_info_is_sched(s)) return 0;

	t = sl_thd_initaep_alloc(cap_info_dci(rs), NULL, 0, 0, 0, 0, 0);
	if (!t) return 0;
	/* child is not a scheduler, don't copy into child */
	/* parent only needs the thdcap */
	thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!thdcap) goto err;

	cap_info_thd_init(rc, t, 0);
	cap_info_initthd_init(rs, t, 0);
	cap_info_cpu_local(rs)->p_initthdcap = thdcap;
	cap_info_cpu_local(rs)->initthdid    = *tid = sl_thd_thdid(t);

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_initaep_create_cserialized(u32_t *sndtidret, u32_t *rcvtcret, u32_t spdid_owntc, u32_t key_ipimax, u32_t ipiwin32b)
{
	spdid_t                 cur     = cos_inv_token(), s = spdid_owntc >> 16;
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *t       = NULL, *rinit = NULL;
	thdcap_t                thdcap  = 0;
	int                     owntc   = (spdid_owntc << 16) >> 16;
	cos_channelkey_t        key     = key_ipimax >> 16;
	u32_t                   ipimax  = (key_ipimax << 16) >> 16;
	microsec_t              ipiwin  = (microsec_t)ipiwin32b;
	int                     ret;
	tcap_t                  tc;
	arcvcap_t               rcv;
	asndcap_t               snd;
	thdid_t                 tid;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (!cap_info_is_sched(s)) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;
	t = sl_thd_initaep_alloc(cap_info_dci(rs), rinit, 1, owntc, 0, 0, 0);
	if (!t) return 0;
	/* child is a scheduler.. copy initcaps */
	ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, cap_ci, sl_thd_thdcap(t));
	if (ret) goto err;
	ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cap_ci, sl_thd_rcvcap(t));
	if (ret) goto err;
	if (owntc) {
		ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, cap_ci, sl_thd_tcap(t));
		if (ret) goto err;
	} else {
		/* if it's a scheduler.. use parent's tcap (current spdid) */
		ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, cap_ci, sl_thd_tcap(rinit));
		if (ret) goto err;
	}

	/* parent needs tcap/rcv to manage time. thd/asnd to activate. */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) goto err;
	rcv = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
	if (!rcv) goto err;
	tc = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_TCAP, sl_thd_tcap(t));
	if (!tc) goto err;
	snd = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ASND, sl_thd_asndcap(t));
	if (!snd) goto err;

	cap_comminfo_init(t, ipiwin, ipimax);
	cap_info_thd_init(rc, t, key);
	cap_info_initthd_init(rs, t, 0);
	cap_info_cpu_local(rs)->p_initthdcap = thdcap = ret;
	cap_info_cpu_local(rs)->initthdid    = tid = sl_thd_thdid(t);
	*rcvtcret  = (rcv << 16) | (tc);
	*sndtidret = (snd << 16) | (tid);

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_aep_create_ext_cserialized(u32_t *drcvtidret, u32_t *rcvtcret, u32_t owntc_spdid_thdidx, u32_t chkey_ipimax, u32_t ipiwin32b)
{
	spdid_t                 cur     = cos_inv_token();
	spdid_t                 s       = (owntc_spdid_thdidx << 1) >> 17;
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	thdclosure_index_t      tidx    = (owntc_spdid_thdidx << 16) >> 16;
	int                     owntc   = (owntc_spdid_thdidx >> 30);
	struct sl_thd          *t       = NULL, *rinit = NULL;
	thdcap_t                thdcap  = 0;
	cos_channelkey_t        key     = chkey_ipimax >> 16;
	u32_t                   ipimax  = (chkey_ipimax << 16) >> 16;
	microsec_t              ipiwin  = (microsec_t)ipiwin32b;
	arcvcap_t               srcrcv, dstrcv;
	tcap_t                  tc;
	int                     ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (tidx <= 0) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;

	t = sl_thd_aep_alloc_ext(cap_info_dci(rs), rinit, tidx, 1, owntc, 0, 0, 0, &srcrcv);
	if (!t) return 0;
	/* cur is a scheduler, copy thdcap */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) goto err;
	/*
	 * for aep thread.. rcv cap should be accessible in the destination component,
	 * so we return that cap so the scheduler can init proper structucap of the dest component.
	 */
	dstrcv = cos_cap_cpy(cap_info_ci(rs), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
	if (!dstrcv) goto err;

	if (owntc) {
		/*
		 * needs access to rcvcap if it's doing tcap transfer
		 * complexity: sl data-structure to keep both rcvs? one to return to user, one to keep!
		 */
		srcrcv = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
		if (!srcrcv) goto err;

		tc = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_TCAP, sl_thd_tcap(t));
		if (!tc) goto err;

		/* TODO: size check before packing */
		*rcvtcret = (srcrcv << 16) | (tc);
	} else {
		/* copy sched tc (offset) pcapumably INITTCAP */
		*rcvtcret = BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE;
	}

	cap_comminfo_init(t, ipiwin, ipimax);
	cap_info_thd_init(rc, t, key);
	cap_info_thd_init(rs, t, 0);
	*drcvtidret = (dstrcv << 16 | sl_thd_thdid(t));
	thdcap = ret;

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_aep_create_cserialized(thdid_t *tid, u32_t *tcrcvret, u32_t owntc_tidx, u32_t key_ipimax, u32_t ipiwin32b)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	thdclosure_index_t      tidx    = (owntc_tidx << 16) >> 16;
	int                     owntc   = owntc_tidx >> 16;
	cos_channelkey_t        key     = key_ipimax >> 16;
	u32_t                   ipimax  = (key_ipimax << 16) >> 16;
	microsec_t              ipiwin  = (microsec_t)ipiwin32b;
	struct sl_thd          *t       = NULL, *rinit = NULL;
	thdcap_t                thdcap  = 0;
	arcvcap_t               rcv;
	tcap_t                  tc;
	int                     ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!cap_info_is_sched(cur)) return 0;
	if (tidx <= 0) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;

	t = sl_thd_aep_alloc_ext(cap_info_dci(rc), rinit, tidx, 1, owntc, 0, 0, 0, &rcv);
	if (!t) return 0;
	/* current is a sched, so copy */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) goto err;

	rcv = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
	if (!rcv) goto err;

	if (owntc) {
		tc = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_TCAP, sl_thd_tcap(t));
		if (!tc) goto err;
	} else {
		/* copy sched tc (offset) pcapumably INITTCAP */
		tc = BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE;
	}

	cap_comminfo_init(t, ipiwin, ipimax);
	cap_info_thd_init(rc, t, key);
	*tcrcvret = (tc << 16 | rcv);
	*tid      = sl_thd_thdid(t);
	thdcap    = ret;

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_thd_retrieve_cserialized(thdid_t *inittid, int *unused, spdid_t s, thdid_t tid)
{
	spdid_t                   cur     = cos_inv_token();
	struct cos_defcompinfo   *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo      *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info     *rc      = cap_info_comp_find(cur);
	struct cap_comp_info     *rs      = cap_info_comp_find(s);
	struct sl_thd            *ti      = cap_info_thd_find(rs, tid);
	struct cap_comp_cpu_info *rs_cpu  = NULL;
	thdcap_t                  thdcap  = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (!ti || !sl_thd_thdcap(ti)) return 0;
	rs_cpu = cap_info_cpu_local(rs);

	if (tid == rs_cpu->initthdid) {
		thdcap = rs_cpu->p_initthdcap;
	} else {
		thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(ti));
		if (!thdcap) goto err;
		cap_info_thd_init(rc, ti, 0);
	}
	*inittid = rs_cpu->initthdid;

	return thdcap;
err:
	return 0;
}

thdcap_t
capmgr_thd_retrieve_next_cserialized(thdid_t *tid, int *unused, spdid_t s)
{
	spdid_t                   cur     = cos_inv_token();
	struct cos_defcompinfo   *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo      *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info     *rc      = cap_info_comp_find(cur);
	struct cap_comp_info     *rs      = cap_info_comp_find(s);
	struct sl_thd            *ti      = NULL;
	struct cap_comp_cpu_info *rs_cpu  = NULL;
	thdcap_t                  thdcap  = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	ti = cap_info_thd_next(rs);
	if (ti == NULL) return 0;
	rs_cpu = cap_info_cpu_local(rs);

	if (sl_thd_thdid(ti) == rs_cpu->initthdid) {
		thdcap = rs_cpu->p_initthdcap;
	} else {
		thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(ti));
		if (!thdcap) goto err;
		/* add to parent's array, for grand-parent's walk-through */
		cap_info_thd_init(rc, ti, 0);
	}
	*tid = sl_thd_thdid(ti);

	return thdcap;
err:
	return 0;
}

arcvcap_t
capmgr_rcv_create_cserialized(u32_t spd_tid, u32_t key_ipimax, u32_t ipiwin32b)
{
	spdid_t                 cur     = cos_inv_token(), s = (spd_tid >> 16);
	thdid_t                 tid     = (spd_tid << 16) >> 16;
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_find(rs, tid), *rinit = NULL;
	arcvcap_t               rcv     = 0, rcvret = 0;
	struct cap_comm_info   *comm    = NULL;
	cos_channelkey_t        key     = (key_ipimax >> 16);
	microsec_t              ipiwin  = (microsec_t)ipiwin32b;
	u32_t                   ipimax  = (key_ipimax << 16) >> 16;
	tcap_t                  tc      = 0;

	if (!key) return 0;
	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (sl_thd_rcvcap(ti)) return 0;
	/* if it's not the same component.. s must not be a scheduler and cur should be a scheduler */
	if (cur != s && (!cap_info_is_sched(cur) || cap_info_is_sched(s))) return 0;
	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;

	comm = cap_comm_tid_lkup(sl_thd_thdid(ti));
	if (comm && comm->rcvcap) return 0;
	tc = sl_thd_tcap(ti);
	if (!tc) tc = sl_thd_tcap(rinit);
	assert(tc);
	rcv = cos_arcv_alloc(cap_ci, sl_thd_thdcap(ti), tc, cap_ci->comp_cap, sl_thd_rcvcap(rinit));
	if (!rcv) return 0;
	sl_thd_aepinfo(ti)->rcv = rcv;

	rcvret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, rcv);
	if (!rcvret) return 0;

	cap_comminfo_init(ti, ipiwin, ipimax);
	cap_channelaep_set(key, ti);

	return rcvret;
}

asndcap_t
capmgr_asnd_create(spdid_t s, thdid_t tid /* thd with rcvcap */)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_find(rs, tid);
	capid_t                 cap     = 0, capret = 0;
	cap_t                   type    = 0;
	struct cap_comm_info   *comm    = NULL;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!ti || !sl_thd_rcvcap(ti)) return 0;
	/* either scheduler creates the sndcap or the component creates itself as it has access to rcvcap */
	if (!cap_info_is_sched(cur) && cur != s) return 0;
	comm = cap_comm_tid_lkup(sl_thd_thdid(ti));
	if (!comm) return 0;

	type = cap_comminfo_xcoresnd_create(comm, &cap);
	if (!type || !cap) return 0;
	capret = cos_cap_cpy(cap_info_ci(rc), cap_ci, type, cap);

	return (asndcap_t)capret;
}

asndcap_t
capmgr_asnd_rcv_create(arcvcap_t rcv)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	capid_t                 cap     = 0, capret = 0;
	cap_t                   type    = 0;
	struct cap_comm_info   *comm    = NULL;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!cap_info_is_sched(cur)) return 0;
	comm = cap_comm_rcv_lkup(cap_info_ci(rc), rcv);
	if (!comm || !comm->rcvcap) return 0;

	type = cap_comminfo_xcoresnd_create(comm, &cap);
	if (!type || !cap) return 0;
	capret = cos_cap_cpy(cap_info_ci(rc), cap_ci, type, cap);

	return (asndcap_t)capret;
}

asndcap_t
capmgr_asnd_key_create(cos_channelkey_t key)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	capid_t                 cap     = 0, capret = 0;
	cap_t                   type    = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!key) return 0;
	type = cap_channelaep_asnd_get(key, &cap);
	if (!cap || !type) return 0;
	capret = cos_cap_cpy(cap_info_ci(rc), cap_ci, type, cap);

	return (asndcap_t)capret;
}
