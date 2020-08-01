#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

CREGPARM(1) int
__cosrt_c_cosrtdefault(struct usr_inv_cap *uc, long p0, long p1, long p2, long p3)
{
	return cos_sinv(uc->cap_no, p0, p1, p2, p3);
}

CREGPARM(1) int
__cosrt_c_cosrtretdefault(struct usr_inv_cap *uc, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)
{
	return cos_sinv_2rets(uc->cap_no, p0, p1, p2, p3, r1, r2);
}
