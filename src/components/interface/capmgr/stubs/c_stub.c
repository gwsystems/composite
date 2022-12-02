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

COS_CLIENT_STUB(arcvcap_t, capmgr_rcv_create, spdid_t child, thdid_t tid, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	COS_CLIENT_INVCAP;
	word_t spd_tid    = (child << 16) | tid;
	word_t key_ipimax = (key << 16) | ipimax;
	word_t ipiwin32b  = (u32_t)ipiwin;

	return cos_sinv(uc, spd_tid, key_ipimax, ipiwin32b, 0);
}

COS_CLIENT_STUB(thdcap_t, capmgr_initthd_create, spdid_t child, thdid_t *tid)
{
	COS_CLIENT_INVCAP;
	word_t unused, tid_ret;
	thdcap_t ret;

	ret = cos_sinv_2rets(uc, child, 0, 0, 0, &tid_ret, &unused);
	*tid = tid_ret;

	return ret;
}

COS_CLIENT_STUB(thdcap_t, capmgr_thd_create_thunk, thdclosure_index_t id, thdid_t *tid)
{
	COS_CLIENT_INVCAP;
	word_t unused, tid_ret;
	thdcap_t ret;

	ret = cos_sinv_2rets(uc, id, 0, 0, 0, &tid_ret, &unused);
	*tid = tid_ret;

	return ret;
}

COS_CLIENT_STUB(thdcap_t, capmgr_thd_create_ext, spdid_t child, thdclosure_index_t idx, thdid_t *tid)
{
	COS_CLIENT_INVCAP;
	word_t unused, tid_ret;
	thdcap_t ret;

	ret = cos_sinv_2rets(uc, child, idx, 0, 0, &tid_ret, &unused);
	*tid = tid_ret;

	return ret;
}

COS_CLIENT_STUB(thdcap_t, capmgr_aep_create_thunk, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	COS_CLIENT_INVCAP;
	word_t tcrcvret   = 0;
	thdcap_t thd     = 0;
	thdid_t tid      = 0;
	u32_t owntc_idx  = (owntc << 16) | idx;
	u32_t key_ipimax = (key << 16) | ((ipimax << 16) >> 16);
	u32_t ipiwin32b  = (u32_t)ipiwin;

	if (idx < 1) return 0;

	thd = cos_sinv_2rets(uc, owntc_idx, key_ipimax, ipiwin32b, 0, &tid, &tcrcvret);
	if (!thd) return 0;

	aep->thd  = thd;
	aep->rcv  = (tcrcvret << 16) >> 16;
	aep->tc   = (tcrcvret >> 16);
	aep->tid  = tid;

	return thd;
}

COS_CLIENT_STUB(thdcap_t, capmgr_aep_create_ext, spdid_t child, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv)
{
	COS_CLIENT_INVCAP;
	word_t drcvtidret  = 0;
	word_t tcrcvret    = 0;
	thdcap_t thd       = 0;
	u32_t spdid_thdidx = (child << 16) | ((idx << 16) >> 16);
	u32_t key_ipimax   = (key << 16) | ((ipimax << 16) >> 16);
	u32_t ipiwin32b    = (u32_t)ipiwin;

	thd = cos_sinv_2rets(uc, spdid_thdidx, owntc, key_ipimax, ipiwin32b, &drcvtidret, &tcrcvret);
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

	thd = cos_sinv_2rets(uc, child_owntc, key_ipimax, ipiwin32b, 0, &sndtidret, &rcvtcret);
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
