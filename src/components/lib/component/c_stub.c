#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <c_stub.h>

COS_CLIENT_STUB(int, cosrtdefault, long p0, long p1, long p2, long p3)
{
	COS_CLIENT_INVCAP;

	return cos_sinv(uc->cap_no, p0, p1, p2, p3);
}

COS_CLIENT_STUB(int, cosrtretdefault, long p0, long p1, long p2, long p3, word_t *r1, word_t *r2)
{
	COS_CLIENT_INVCAP;

	return cos_sinv_2rets(uc->cap_no, p0, p1, p2, p3, r1, r2);
}
