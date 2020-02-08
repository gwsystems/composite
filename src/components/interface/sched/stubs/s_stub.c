#include <cos_stubs.h>
#include <sched.h>

COS_SERVER_3RET_STUB(int, sched_thd_block_timeout)
{
	cycles_t elapsed = 0;

	elapsed = sched_thd_block_timeout((thdid_t)p0, ((cycles_t)p1 << 32 | (cycles_t)p2));
	*r1 = (elapsed >> 32);
	*r2 = (elapsed << 32) >> 32;

	return 0;
}

COS_SERVER_3RET_STUB(thdid_t, sched_aep_create_closure)
{
	struct cos_defcompinfo *dci;
	int                 owntc  = (p0 << 16) >> 16;
	thdclosure_index_t  idx    = (p0 >> 16);
	microsec_t          ipiwin = (microsec_t)p2;
	u32_t               ipimax = (p1 << 16) >> 16;
	cos_channelkey_t    key    = (p1 >> 16);
	arcvcap_t rcv;
	thdid_t ret;

	ret = sched_aep_create_closure(idx, owntc, key, ipiwin, ipimax, &rcv);

	*r1 = rcv;
	*r2 = 0;

	return ret;
}
