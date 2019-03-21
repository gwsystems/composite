#include <cos_stubs.h>
#include <pong.h>

COS_SERVER_3RET_STUB(int, call_argsrets)
{
	return call_argsrets((int)p0, (int)p1, (int)p2, (int)p3, (int *)r1, (int *)r2);
}

COS_SERVER_3RET_STUB(int, call_subset)
{
	return call_subset((unsigned long)p0, (unsigned long)p1, (unsigned long *)r1);
}

COS_SERVER_3RET_STUB(thdid_t, call_ids)
{
	return call_ids((compid_t *)r1, (compid_t *)r2);
}
