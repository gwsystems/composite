#include <cos_stubs.h>
#include <capmgr.h>

COS_SERVER_3RET_STUB(arcvcap_t, capmgr_rcv_create)
{
	spdid_t child   = p0;
	thdcap_t thdcap = p1;

	return capmgr_rcv_create(child, thdcap);
}


COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_create_thunk)
{
	thdid_t retthd = 0;
	struct cos_dcb_info *retdcb;
	thdcap_t ret;

	ret = capmgr_thd_create_thunk(p0, &retthd, &retdcb);
	*r1 = retthd;
	*r2 = (word_t)retdcb;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_create_ext)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_thd_create_ext(p0, p1, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(int, capmgr_retrieve_dcbinfo)
{
	thdid_t ret;
	arcvcap_t arcv = 0;
	asndcap_t asnd = 0;
	struct cos_dcb_info *retdcb;

	ret = capmgr_retrieve_dcbinfo(p0, &arcv, &asnd, &retdcb);
#if defined(__x86_64__)
	assert(arcv < ((1 << 32) - 1));
	*r1 = (word_t)arcv << 32 | asnd;
#else
	assert(arcv < ((1 << 16) - 1));
	*r1 = arcv << 16 | asnd;
#endif
	printc("----arcv: %d, asnd: %d, %lx\n", arcv, asnd, *r1);
	*r2 = (word_t)retdcb;

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

	assert(0);
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
	struct cos_dcb_info    *retdcb;
	asndcap_t               snd;
	thdcap_t                thdtidret;

	assert(0);
	thdtidret = capmgr_aep_create_thunk(&aep, thunk, owntc, key, ipimax, ipiwin32b, &retdcb);
	*r1 = (word_t)retdcb;
	*r2 = (aep.rcv << 16) | aep.tc;
	assert(0);

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
	assert(0);

	assert(0);
	ret = capmgr_aep_create_ext(child, &aep, idx, owntc, key, ipiwin, ipimax, &dcbret, &extrcv);
	*r1 = aep.tid | (extrcv << 16);
	*r2 = (aep.rcv << 16) | aep.tc;

	return ret;
}
