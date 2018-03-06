#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <capmgr.h>
#include "cap_info.h"

thdcap_t
capmgr_thd_create_intern(spdid_t cur, int idx)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *r       = cap_info_comp_find(cur);
	struct sl_thd *rt = NULL, *t = NULL;
	int ret;

	if (!r || !cap_info_init_check(r)) return 0;
	if (!cap_info_is_sched(cur)) return 0;
	if (idx <= 0) return 0;

	t = sl_thd_ext_idx_alloc(cap_info_dci(r), idx);
	if (!t) return 0;
	cap_info_thd_init(r, t);

	ret = cos_cap_cpy(cap_info_ci(r), cap_ci, CAP_THD, sl_thd_thdcap(t));

	return ret;
}

thdcap_t
capmgr_ext_thd_create_intern(spdid_t cur, spdid_t s, int idx)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd *t = NULL;
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (cap_info_is_sched(s)) return 0;
	if (idx <= 0) return 0;

	t = sl_thd_ext_idx_alloc(cap_info_dci(rs), idx);
	if (!t) return 0;
	cap_info_thd_init(rc, t);
	cap_info_thd_init(rs, t);

	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	/* child is not a scheduler, don't copy into child */

	return ret;
}

thdcap_t
capmgr_initthd_create_intern(spdid_t cur, spdid_t s)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd *t = NULL;
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (cap_info_is_sched(s)) return 0;

	t = sl_thd_child_initaep_alloc(cap_info_dci(rs), 0, 0);
	if (!t) return 0;
	cap_info_thd_init(rc, t);
	cap_info_initthd_init(rs, t);

	/* child is not a scheduler, don't copy into child */
	/* parent only needs the thdcap */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) return 0;

	rs->p_initthdcap = ret;
	rs->initthdid    = t->thdid;

	return ret;
}

thdcap_t
capmgr_initaep_create_intern(spdid_t cur, spdid_t s, int owntc, int u1, asndcap_t *sndret, u32_t *rcvtcret)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd *t = NULL, *rinit = NULL;
	tcap_t tc;
	arcvcap_t rcv;
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (!cap_info_is_sched(s)) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;
	t = sl_thd_ext_child_initaep_alloc(cap_info_dci(rs), rinit, 1);
	if (!t) return 0;
	cap_info_thd_init(rc, t);
	cap_info_initthd_init(rs, t);

	/* child is a scheduler.. copy initcaps */
	ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITTHD_BASE, cap_ci, sl_thd_thdcap(t));
	if (ret) return 0;
	ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITRCV_BASE, cap_ci, sl_thd_rcvcap(t));
	if (ret) return 0;
	if (owntc) {
		ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITTCAP_BASE, cap_ci, sl_thd_tcap(t));
		if (ret) return 0;
	} else {
		/* if it's a scheduler.. use parent's tcap (current spdid) */
		ret = cos_cap_cpy_at(cap_info_ci(rs), BOOT_CAPTBL_SELF_INITTCAP_BASE, cap_ci, sl_thd_tcap(rinit));
		if (ret) return 0;
	}

	/* parent needs tcap/rcv to manage time. thd/asnd to activate. */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) return 0;

	rs->p_initthdcap = ret;
	rs->initthdid    = t->thdid;
	rcv = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
	if (!rcv) return 0;
	tc = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_TCAP, sl_thd_tcap(t));
	if (!tc) return 0;

	*rcvtcret = (rcv << 16) | (tc);
	*sndret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ASND, sl_thd_asndcap(t));

	return ret;
}

thdcap_t
capmgr_ext_aep_create_intern(spdid_t cur, spdid_t s, int tidx, int owntc, arcvcap_t *dstrcvret, u32_t *rcvtcret)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd *t = NULL, *rinit = NULL;
	arcvcap_t srcrcv, dstrcv;
	tcap_t tc;
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (tidx <= 0) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;

	t = sl_thd_extaep_idx_alloc(cap_info_dci(rs), rinit, tidx, owntc, &srcrcv);
	if (!t) return 0;
	cap_info_thd_init(rc, t);
	cap_info_thd_init(rs, t);

	/* cur is a scheduler, copy thdcap */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) return 0;
	/*
	 * for aep thread.. rcv cap should be accessible in the destination component,
	 * so we return that cap so the scheduler can init proper structucap of the dest component.
	 */
	*dstrcvret = cos_cap_cpy(cap_info_ci(rs), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
	if (!(*dstrcvret)) return 0;

	if (owntc) {
		/*
		 * needs access to rcvcap if it's doing tcap transfer
		 * complexity: sl data-structure to keep both rcvs? one to return to user, one to keep!
		 */
		srcrcv = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
		if (!srcrcv) return 0;

		tc = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_TCAP, sl_thd_tcap(t));
		if (!tc) return 0;

		/* TODO: size check before packing */
		*rcvtcret = (srcrcv << 16) | (tc);
	} else {
		/* copy sched tc (offset) pcapumably INITTCAP */
		*rcvtcret = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	}

	return ret;
}

thdcap_t
capmgr_aep_create_intern(spdid_t cur, int tidx, int owntc, int u1, arcvcap_t *rcvret, tcap_t *tcret)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct sl_thd *t = NULL, *rinit = NULL;
	arcvcap_t rcv;
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!cap_info_is_sched(cur)) return 0;
	if (tidx <= 0) return 0;

	rinit = cap_info_initthd(rc);
	if (!rinit) return 0;

	t = sl_thd_extaep_idx_alloc(cap_info_dci(rc), rinit, tidx, owntc, &rcv);
	if (!t) return 0;
	cap_info_thd_init(rc, t);

	/* current is a sched, so copy */
	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(t));
	if (!ret) return 0;

	*rcvret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ARCV, sl_thd_rcvcap(t));
	if (!(*rcvret)) return 0;

	if (owntc) {
		*tcret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_TCAP, sl_thd_tcap(t));
	} else {
		/* copy sched tc (offset) pcapumably INITTCAP */
		*tcret = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	}

	return ret;
}

thdcap_t
capmgr_thd_retrieve_intern(spdid_t cur, spdid_t s, thdid_t tid)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_find(rs, tid);
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;
	if (!ti || !sl_thd_thdcap(ti)) return 0;

	if (tid == rs->initthdid) {
		ret = rs->p_initthdcap;
		goto done;
	}

	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(ti));

done:
	return ret;
}

thdid_t
capmgr_thd_retrieve_next_intern(spdid_t cur, spdid_t s, int u1, int u2, thdcap_t *thdcap, int *u3)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_next(rs);

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!cap_info_is_sched(cur) || !cap_info_is_child(rc, s)) return 0;

	if (ti == NULL) return 0;
	if (ti->thdid == rs->initthdid) {
		*thdcap = rs->p_initthdcap;
		goto done;
	}
	*thdcap = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_THD, sl_thd_thdcap(ti));
	if (!(*thdcap)) return 0;
	/* add to parent's array, for grand-parent's walk-through */
	cap_info_thd_init(rc, ti);

done:
	return ti->thdid;
}

/* TODO: use thdid? or rcvcap? */
asndcap_t
capmgr_asnd_create_intern(spdid_t cur, spdid_t s, thdid_t tid /* thd with rcvcap */)
{
	struct cos_defcompinfo *cap_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *cap_ci  = cos_compinfo_get(cap_dci);
	struct cap_comp_info   *rc      = cap_info_comp_find(cur);
	struct cap_comp_info   *rs      = cap_info_comp_find(s);
	struct sl_thd          *ti      = cap_info_thd_find(rs, tid);
	asndcap_t snd;
	int ret;

	if (!rc || !cap_info_init_check(rc)) return 0;
	if (!rs || !cap_info_init_check(rs)) return 0;
	if (!ti || !sl_thd_rcvcap(ti)) return 0;
	/* either scheduler creates the sndcap or the component creates itself as it has access to rcvcap */
	if (!cap_info_is_sched(cur) && cur != s) return 0;

	snd = cos_asnd_alloc(cap_ci, sl_thd_rcvcap(ti), cap_ci->captbl_cap);
	if (!snd) return 0;

	ret = cos_cap_cpy(cap_info_ci(rc), cap_ci, CAP_ASND, snd);

	return ret;
}
