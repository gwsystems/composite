#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <resmgr.h>
#include "res_info.h"

thdcap_t
resmgr_thd_create_intern(spdid_t cur, int idx)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *r = res_info_comp_find(cur);
	struct res_thd_info *rt = NULL;
	struct sl_thd *t = NULL;
	int ret;

	assert(res_info_init_check(r));
	/* TODO: check if it's a scheduler. */
	t = sl_thd_ext_idx_alloc(res_info_dci(r), idx);
	assert(t);
	rt = res_info_thd_init(r, t);
	assert(rt);

	ret = cos_cap_cpy(res_info_ci(r), res_ci, CAP_THD, res_thd_thdcap(rt));
	assert(ret > 0);

	return ret;
}

thdcap_t
resmgr_ext_thd_create_intern(spdid_t cur, spdid_t s, int idx)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_comp_info *rs = res_info_comp_find(s);
	struct res_thd_info *rt = NULL, *rst = NULL;
	struct sl_thd *t = NULL;
	int ret;

	assert(rc && res_info_init_check(rc));
	assert(rs);
	/* TODO: only called by a scheduling component of a non-scheduling "child" component */
	/* assert(rc->is_sched && !(rc->is_sched)); */
	/* TODO: check if it's a scheduler. */
	t = sl_thd_ext_idx_alloc(res_info_dci(rs), idx);
	assert(t);
	rt = res_info_thd_init(rc, t);
	assert(rt);
	rst = res_info_thd_init(rs, t);
	assert(rst);

	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_THD, res_thd_thdcap(rt));
	assert(ret > 0);
	/* not copied into the child component's address-space */

	return ret;
}

thdcap_t
resmgr_initthd_create(spdid_t cur, spdid_t s)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_comp_info *rs = res_info_comp_find(s);
	struct res_thd_info *rt = NULL, *rst = NULL;
	struct sl_thd *t = NULL;
	int ret;

	assert(res_info_init_check(rc) && res_info_init_check(rs));
	/* TODO: are you the parent of this component? */
	/* cur == r->parent->cid ? */
	t = sl_thd_child_initaep_alloc(res_info_dci(rs), 0, 0);
	assert(t);
	rt = res_info_thd_init(rc, t);
	assert(rt);
	rst = res_info_initthd_init(rs, t);
	assert(rst);

	ret = cos_cap_cpy_at(res_info_ci(rs), BOOT_CAPTBL_SELF_INITTHD_BASE, res_ci, res_thd_thdcap(rt));
	assert(ret == 0);

	/* parent only needs the thdcap */
	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_THD, res_thd_thdcap(rt));
	assert(ret > 0);

	return ret;
}

thdcap_t
resmgr_initaep_create_intern(spdid_t cur, spdid_t s, int owntc, asndcap_t *sndret, u32_t *unused)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_comp_info *rs = res_info_comp_find(s);
	struct res_thd_info *rt = NULL, *rst = NULL, *rinit = NULL;
	struct sl_thd *t = NULL, *sched = NULL;
	int ret;

	assert(res_info_init_check(rc) && res_info_init_check(rs));
	rinit = res_info_initthd(rc);
	assert(rinit);
	sched = rinit->schthd;
	assert(sched);
	/* TODO: are you the parent of this component? */
	/* cur == r->parent->cid ? */
	t = sl_thd_ext_child_initaep_alloc(res_info_dci(rs), sched, 1);
	assert(t);
	rt = res_info_thd_init(rc, t);
	assert(rt);
	rst = res_info_initthd_init(rs, t);	
	assert(rst);

	/* whether to copy or not is not clear.. perhaps depending on whether it's a scheduler or not */
	ret = cos_cap_cpy_at(res_info_ci(rs), BOOT_CAPTBL_SELF_INITTHD_BASE, res_ci, res_thd_thdcap(rt));
	assert(ret == 0);
	ret = cos_cap_cpy_at(res_info_ci(rs), BOOT_CAPTBL_SELF_INITRCV_BASE, res_ci, res_thd_rcvcap(rt));
	assert(ret == 0);
	if (owntc) {
		ret = cos_cap_cpy_at(res_info_ci(rs), BOOT_CAPTBL_SELF_INITTCAP_BASE, res_ci, res_thd_tcap(rt));
		assert(ret == 0);
	} else {
		/* if it's a scheduler.. use parent's tcap (current spdid) */
		ret = cos_cap_cpy_at(res_info_ci(rs), BOOT_CAPTBL_SELF_INITTCAP_BASE, res_ci, sl_thd_tcap(sched));
		assert(ret == 0);
	}

	/* parent only needs the thdcap/asndcap */
	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_THD, res_thd_thdcap(rt));
	assert(ret > 0);
	*sndret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_ASND, res_thd_asndcap(rt));
	assert(*sndret > 0);

	return ret;
}

thdcap_t
resmgr_ext_aep_create_intern(spdid_t cur, spdid_t s, int tidx, int owntc, arcvcap_t *dstrcvret, u32_t *rcvtcret)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_comp_info *rs = res_info_comp_find(s);
	struct res_thd_info *rt = NULL, *rst = NULL, *rinit = NULL;
	struct sl_thd *t = NULL, *sched = NULL;
	arcvcap_t srcrcv, dstrcv;
	tcap_t tc;
	int ret;

	assert(res_info_init_check(rc));
	rinit = res_info_initthd(rc);
	assert(rinit);
	sched = rinit->schthd;
	assert(sched);

	t = sl_thd_extaep_idx_alloc(res_info_dci(rc), sched, tidx, owntc);
	assert(t);
	rt = res_info_thd_init(rc, t);
	assert(rt);
	rst = res_info_thd_init(rs, t);
	assert(rst);

	/* whether to copy or not is not clear.. perhaps depending on whether it's a scheduler or not */
	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_THD, res_thd_thdcap(rt));
	assert(ret > 0);
	/* 
	 * for aep thread.. rcv cap should be accessible in the destination component,
	 * so we return that cap so the scheduler can init proper structures of the dest component.
	 */
	*dstrcvret = cos_cap_cpy(res_info_ci(rs), res_ci, CAP_ARCV, res_thd_rcvcap(rt));
	assert(*dstrcvret > 0);
	if (owntc) {
		/*
		 * needs access to rcvcap if it's doing tcap transfer
		 * complexity: sl data-structure to keep both rcvs? one to return to user, one to keep!
		 */
		srcrcv = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_ARCV, res_thd_rcvcap(rt));
		assert(srcrcv > 0);

		tc = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_TCAP, res_thd_tcap(rt));
		assert(tc > 0);

		/* TODO: size check before packing */
		*rcvtcret = (srcrcv << 16) | (tc);
	} else {
		/* copy sched tc (offset) presumably INITTCAP */
		*rcvtcret = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	}

	return ret;
}

thdcap_t
resmgr_aep_create_intern(spdid_t cur, int tidx, int owntc, arcvcap_t *rcvret, tcap_t *tcret)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_thd_info *rt = NULL, *rst = NULL, *rinit = NULL;
	struct sl_thd *t = NULL, *sched = NULL;
	int ret;

	assert(rc && res_info_init_check(rc));
	/* TODO: only called by a scheduling component of a non-scheduling "child" component */
	/* assert(rc->is_sched && !(rc->is_sched)); */
	rinit = res_info_initthd(rc);
	assert(rinit);
	sched = rinit->schthd;
	assert(sched);

	t = sl_thd_extaep_idx_alloc(res_info_dci(rc), sched, tidx, owntc);
	assert(t);
	rt = res_info_thd_init(rc, t);
	assert(rt);

	/* whether to copy or not is not clear.. perhaps depending on whether it's a scheduler or not */
	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_THD, res_thd_thdcap(rt));
	assert(ret > 0);
	*rcvret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_ARCV, res_thd_rcvcap(rt));
	assert(*rcvret > 0);
	if (owntc) {
		*tcret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_TCAP, res_thd_tcap(rt));
		assert(*tcret > 0);
	} else {
		/* copy sched tc (offset) presumably INITTCAP */
		*tcret = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	}

	return ret;
}

thdcap_t
resmgr_thd_retrieve(spdid_t cur, spdid_t s, thdid_t tid)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_comp_info *rs = res_info_comp_find(s);
	struct res_thd_info *ti = res_info_thd_find(rs, tid);
	int ret;

	assert(res_info_init_check(rc) && res_info_init_check(rs));
	assert(ti && res_thd_thdcap(ti));
	/* TODO: are you the parent of that component? */

	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_THD, res_thd_thdcap(ti));
	assert(ret > 0);

	return ret;
}

/* TODO: use thdid? or rcvcap? */
asndcap_t
resmgr_asnd_create(spdid_t cur, spdid_t s, thdid_t tid /* thd with rcvcap */)
{
	struct cos_defcompinfo *res_dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *res_ci     = cos_compinfo_get(res_dci);
	struct res_comp_info *rc = res_info_comp_find(cur);
	struct res_comp_info *rs = res_info_comp_find(s);
	struct res_thd_info *ti  = res_info_thd_find(rs, tid);
	asndcap_t snd;
	int ret;

	assert(res_info_init_check(rc) && res_info_init_check(rs));
	assert(ti && res_thd_rcvcap(ti));
	/* It does have to be a parent creating.. for being able to access "rcvcap" from a component */

	/* TODO: access check */
	snd = cos_asnd_alloc(res_ci, res_thd_rcvcap(ti), res_info_ci(rs)->captbl_cap);
	assert(snd);

	ret = cos_cap_cpy(res_info_ci(rc), res_ci, CAP_ASND, snd);
	assert(ret > 0);

	return ret;
}
