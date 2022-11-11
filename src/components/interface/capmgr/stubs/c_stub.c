/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <capmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(arcvcap_t, capmgr_rcv_create, spdid_t child, thdcap_t thdcap)
{
	COS_CLIENT_INVCAP;
	word_t spd = child;
	word_t thd = thdcap;

	return cos_sinv(uc->cap_no, spd, thdcap, 0, 0);
}

COS_CLIENT_STUB(thdcap_t, capmgr_initthd_create, spdid_t child, thdid_t *tid)
{
	COS_CLIENT_INVCAP;
	word_t unused, tid_ret;
	thdcap_t ret;

	ret = cos_sinv_2rets(uc->cap_no, child, 0, 0, 0, &tid_ret, &unused);
	*tid = tid_ret;

	return ret;
}

COS_CLIENT_STUB(thdcap_t, capmgr_thd_create_thunk, thdclosure_index_t id, thdid_t *tid, struct cos_dcb_info **dcb)
{
	COS_CLIENT_INVCAP;
	word_t dcb_ret, tid_ret;
	thdcap_t ret;

	ret = cos_sinv_2rets(uc->cap_no, id, 0, 0, 0, &tid_ret, &dcb_ret);
	*dcb = (struct cos_dcb_info *)dcb_ret;
	*tid = tid_ret;

	return ret;
}

COS_CLIENT_STUB(thdcap_t, capmgr_thd_create_ext, spdid_t child, thdclosure_index_t idx, thdid_t *tid)
{
	COS_CLIENT_INVCAP;
	word_t unused, tid_ret;
	thdcap_t ret;

	ret  = cos_sinv_2rets(uc->cap_no, child, idx, 0, 0, &tid_ret, &unused);
	*tid  = tid_ret;

	return ret;
}

COS_CLIENT_STUB(thdid_t, capmgr_retrieve_dcbinfo, thdid_t tid, arcvcap_t *arcv, asndcap_t *asnd, struct cos_dcb_info **dcb)
{
	COS_CLIENT_INVCAP;
	word_t retrs, retdcb;
	thdid_t ret;

	ret = cos_sinv_2rets(uc->cap_no, tid, 0, 0, 0, &retrs, &retdcb);
	*dcb  = (struct cos_dcb_info *)retdcb;
#if defined(__x86_64__)
	*arcv = (retrs >> 32);
	*asnd = (retrs << 32) >> 32;
#else
	*arcv = (retrs >> 16);
	*asnd = (retrs << 16) >> 16;
#endif
	printc("+++++++++++client_arcv: %d, asnd: %d, %lx\n", *arcv, *asnd, retrs);

	return ret;
}

COS_CLIENT_STUB(thdcap_t, capmgr_aep_create_thunk, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, struct cos_dcb_info **dcb)
{
	COS_CLIENT_INVCAP;
	word_t tcrcvret  = 0;
	word_t thdtidret = 0;
	word_t dcb_ret   = 0;
	thdcap_t thd     = 0;
	thdid_t tid      = 0;
	u32_t owntc_idx  = (owntc << 16) | idx;
	u32_t key_ipimax = (key << 16) | ((ipimax << 16) >> 16);
	u32_t ipiwin32b  = (u32_t)ipiwin;

	if (idx < 1) return 0;

	thdtidret = cos_sinv_2rets(uc->cap_no, owntc_idx, key_ipimax, ipiwin32b, 0, &dcb_ret, &tcrcvret);
	if (!thdtidret) return 0;
	thd = thdtidret >> 16;
	tid = (thdtidret << 16) >> 16;
	if (!thd || !tid) return 0;

	aep->thd  = thd;
	aep->rcv  = (tcrcvret << 16) >> 16;
	aep->tc   = (tcrcvret >> 16);
	aep->tid  = tid;

	*dcb = (struct cos_dcb_info *)dcb_ret;

	return thd;
}

/* FIXME: Won't work now */
COS_CLIENT_STUB(thdcap_t, capmgr_aep_create_ext, spdid_t child, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, struct cos_dcb_info **dcb, arcvcap_t *extrcv)
{
	COS_CLIENT_INVCAP;
	word_t drcvtidret  = 0;
	word_t tcrcvret    = 0;
	thdcap_t thd       = 0;
	u32_t spdid_thdidx = (child << 16) | ((idx << 16) >> 16);
	u32_t key_ipimax   = (key << 16) | ((ipimax << 16) >> 16);
	u32_t ipiwin32b    = (u32_t)ipiwin;

	thd = cos_sinv_2rets(uc->cap_no, spdid_thdidx, owntc, key_ipimax, ipiwin32b, &drcvtidret, &tcrcvret);
	if (!thd) return thd;

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = (drcvtidret << 16) >> 16;
	aep->rcv  = tcrcvret >> 16;
	aep->tc   = (tcrcvret << 16) >> 16;
	*extrcv   = drcvtidret >> 16;

	return thd;
}

COS_CLIENT_STUB(thdcap_t, capmgr_initaep_create, spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *snd)
{
	COS_CLIENT_INVCAP;
	u32_t child_owntc = (child << 16) | owntc;
	u32_t key_ipimax  = (key << 16) >> ipimax;
	u32_t ipiwin32b   = (u32_t)ipiwin;
	thdcap_t thd = 0;
	word_t sndtidret = 0, rcvtcret = 0;

	thd = cos_sinv_2rets(uc->cap_no, child_owntc, key_ipimax, ipiwin32b, 0, &sndtidret, &rcvtcret);
	if (!thd) return thd;

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = (sndtidret << 16) >> 16;
	aep->rcv  = rcvtcret >> 16;
	aep->tc   = (rcvtcret << 16) >> 16;
	*snd      = sndtidret >> 16;

	/* initcaps are copied to INITXXX offsets in the dst component */
	return aep->thd;
}
