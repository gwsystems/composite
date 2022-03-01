#include <cos_stubs.h>
#include <capmgr.h>
#include <memmgr.h>

COS_SERVER_3RET_STUB(arcvcap_t, capmgr_rcv_create)
{
	spdid_t child = p0 >> 16;
	thdid_t tid   = (p0 << 16) >> 16;
	cos_channelkey_t key = p1 >> 16;
	microsec_t ipiwin = (p1 << 16) >> 16;
	u32_t ipimax = p2;

	return capmgr_rcv_create(child, tid, key, ipiwin, ipimax);
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_retrieve)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_thd_retrieve(p0, p1, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_retrieve_next)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_thd_retrieve_next(p0, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_create_thunk)
{
	thdid_t retthd = 0;
	struct cos_dcb_info *retdcb;
	thdcap_t ret;

	ret = capmgr_thd_create_thunk(p0, &retthd, &retdcb);
	*r1 = retthd;
	*r2 = &retdcb;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_create_ext)
{
	thdid_t retthd = 0;
	struct cos_dcb_info *retdcb;
	thdcap_t ret;

	ret = capmgr_thd_create_ext(p0, p1, &retthd, &retdcb);
	*r1 = retthd;
	*r2 = &retdcb;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_initthd_create)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_initthd_create(p0, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_initaep_create)
{
	spdid_t                 child  =  p0 >> 16;
	int                     owntc  = (p0 << 16) >> 16;
	cos_channelkey_t        key    =  p1 >> 16;
	u32_t                   ipimax = (p1 << 16) >> 16;
	u32_t                ipiwin32b = (u32_t)p2;
	struct cos_aep_info     aep;
	asndcap_t               snd = 0;
	thdcap_t                thd;

	thd = capmgr_initaep_create(child, &aep, owntc, key, ipimax, ipiwin32b, &snd);
	*r1 = (snd << 16)      | aep.tid;
	*r2 = (aep.rcv << 16) | aep.tc;

	return thd;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_aep_create_thunk)
{
	thdclosure_index_t      thunk  = (p0 << 16) >> 16;
	int                     owntc  =  p0 >> 16;
	cos_channelkey_t        key    =  p1 >> 16;
	u32_t                   ipimax = (p1 << 16) >> 16;
	u32_t                ipiwin32b = (u32_t)p2;
	struct cos_aep_info     aep;
	struct cos_dcb_info    *dcb;
	asndcap_t               snd;
	thdcap_t                thdtidret;

	thdtidret = capmgr_aep_create_thunk(&aep, thunk, owntc, key, ipimax, ipiwin32b, &dcb);
	*r1 = &dcb;
	*r2 = (aep.rcv << 16) | aep.tc;

	return thdtidret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_aep_create_ext)
{
	struct cos_aep_info aep;
	spdid_t child = p0 >> 16;
	thdclosure_index_t idx = ((p0 << 16) >> 16);
	int owntc = p1;
	cos_channelkey_t key = p2 >> 16;
	microsec_t ipiwin = p3;
	u32_t ipimax = ((p2 << 16) >> 16);
	arcvcap_t extrcv = 0;
	struct cos_dcb_info *dcbret;
	thdcap_t ret;

	ret = capmgr_aep_create_ext(child, &aep, idx, owntc, key, ipiwin, ipimax, &dcbret, &extrcv);
	*r1 = aep.tid | (extrcv << 16);
	*r2 = (aep.rcv << 16) | aep.tc;

	return ret;
}
