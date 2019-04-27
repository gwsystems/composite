#include <cos_stubs.h>
#include <pong.h>

COS_SERVER_3RET_STUB(int, pong_argsrets)
{
	return pong_argsrets((int)p0, (int)p1, (int)p2, (int)p3, (int *)r1, (int *)r2);
}

COS_SERVER_3RET_STUB(int, pong_subset)
{
	return pong_subset((unsigned long)p0, (unsigned long)p1, (unsigned long *)r1);
}

COS_SERVER_3RET_STUB(thdid_t, pong_ids)
{
	return pong_ids((compid_t *)r1, (compid_t *)r2);
}
