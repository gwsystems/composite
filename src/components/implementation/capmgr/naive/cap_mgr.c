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

	t = sl_thd_aep_alloc_ext(cap_info_dci(r), NULL, idx, 0, 0, 0, NULL);
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

	t = sl_thd_aep_alloc_ext(cap_info_dci(rs), NULL, idx, 0, 0, 0, NULL);
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

	t = sl_thd_initaep_alloc(cap_info_dci(rs), NULL, 0, 0, 0);
	if (!t) return 0;
	/* child is not a scheduler, don't copy into child */
	/* parent only needs the thdcap */
	thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!thdcap) goto err;

	cap_info_thd_init(rc, t, 0);
	cap_info_initthd_init(rs, t, 0);
	rs->p_initthdcap[cos_cpuid()] = thdcap;
	rs->initthdid[cos_cpuid()]    = *tid = sl_thd_thdid(t);

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_initaep_create_cserialized(u32_t *sndtidret, u32_t *rcvtcret, spdid_t s, int owntc, cos_channelkey_t key)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *t       = NULL, *rinit = NULL;
	thdcap_t                thdcap  = 0;
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
	t = sl_thd_initaep_alloc(cap_info_dci(rs), rinit, 1, owntc, 0);
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

	cap_info_thd_init(rc, t, key);
	cap_info_initthd_init(rs, t, 0);
	rs->p_initthdcap[cos_cpuid()] = thdcap = ret;
	rs->initthdid[cos_cpuid()]    = tid    = sl_thd_thdid(t);
	*rcvtcret  = (rcv << 16) | (tc);
	*sndtidret = (snd << 16) | (tid);

	return thdcap;
err:
	sl_thd_free(t);

	return 0;
}

thdcap_t
capmgr_aep_create_ext_cserialized(u32_t *drcvtidret, u32_t *rcvtcret, spdid_t s, thdclosure_index_t tidx, u32_t owntc_chkey)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *t       = NULL, *rinit = NULL;
	thdcap_t                thdcap  = 0;
	int                     owntc   = (int)(owntc_chkey >> 16);
	cos_channelkey_t        key     = (cos_channelkey_t)((owntc_chkey << 16) >> 16);
	arcvcap_t               srcrcv, dstrcv;
	tcap_t                  tc;
	int                     ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (tidx <= 0) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;

	t = sl_thd_aep_alloc_ext(cap_info_dci(rs), rinit, tidx, 1, owntc, 0, &srcrcv);
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
capmgr_aep_create_cserialized(thdid_t *tid, u32_t *tcrcvret, thdclosure_index_t tidx, int owntc, cos_channelkey_t key)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
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

	t = sl_thd_aep_alloc_ext(cap_info_dci(rc), rinit, tidx, 1, owntc, 0, &rcv);
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
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_find(rs, tid);
	thdcap_t                thdcap  = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (!ti || !sl_thd_thdcap(ti)) return 0;

	if (tid == rs->initthdid[cos_cpuid()]) {
		thdcap   = rs->p_initthdcap[cos_cpuid()];
	} else {
		thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(ti));
		if (!thdcap) goto err;
		cap_info_thd_init(rc, ti, 0);
	}
	*inittid = rs->initthdid[cos_cpuid()];

	return thdcap;
err:
	return 0;
}

thdcap_t
capmgr_thd_retrieve_next_cserialized(thdid_t *tid, int *unused, spdid_t s)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = NULL;
	thdcap_t                thdcap  = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	ti = cap_info_thd_next(rs);
	if (ti == NULL) return 0;

	if (sl_thd_thdid(ti) == rs->initthdid[cos_cpuid()]) {
		thdcap = rs->p_initthdcap[cos_cpuid()];
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

asndcap_t
capmgr_asnd_create(spdid_t s, thdid_t tid /* thd with rcvcap */)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_find(rs, tid);
	asndcap_t               snd     = 0, sndret = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!ti || !sl_thd_rcvcap(ti)) return 0;
	/* either scheduler creates the sndcap or the component creates itself as it has access to rcvcap */
	if (!cap_info_is_sched(cur) && cur != s) return 0;

	snd = cos_asnd_alloc(cap_ci, sl_thd_rcvcap(ti), cap_ci->captbl_cap);
	if (!snd) return 0;
	sndret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ASND, snd);

	return sndret;
}

asndcap_t
capmgr_asnd_rcv_create(arcvcap_t rcv)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	asndcap_t               snd     = 0, sndret = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!cap_info_is_sched(cur)) return 0;

	snd = cos_asnd_alloc(cap_ci, rcv, cap_info_ci(rc)->captbl_cap);
	if (!snd) return 0;
	sndret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ASND, snd);

	return sndret;
}

asndcap_t
capmgr_asnd_key_create(cos_channelkey_t key)
{
	spdid_t                 cur     = cos_inv_token();
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	asndcap_t               snd     = 0, sndret = 0;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!key) return 0;
	snd = cap_channelaep_asnd_get(key);
	if (!snd) return 0;
	sndret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ASND, snd);

	return sndret;
}
