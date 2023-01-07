#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <c_stub.h>

COS_CLIENT_STUB(int, cosrtdefault, word_t p0, word_t p1, word_t p2, word_t p3)
{
	COS_CLIENT_INVCAP;
	return cos_sinv(uc, p0, p1, p2, p3);
}

COS_CLIENT_STUB(int, cosrtretdefault, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)
{
	COS_CLIENT_INVCAP;

	return cos_sinv_2rets(uc, p0, p1, p2, p3, r1, r2);
}
