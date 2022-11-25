#include <cos_stubs.h>
#include <syncipc.h>

COS_SERVER_3RET_STUB(int, syncipc_call)
{
	return syncipc_call((int)p0, p1, p2, r1, r2);
}

COS_SERVER_3RET_STUB(int, syncipc_reply_wait)
{
	return syncipc_reply_wait((int)p0, p1, p2, r1, r2);
}
