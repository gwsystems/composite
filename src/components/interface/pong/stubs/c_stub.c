#include <cos_component.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(int, pong_argsrets)(struct usr_inv_cap *uc, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)
{
	return cos_sinv_2rets(uc->cap_no, p0, p1, p2, p3, r1, r2);
}

COS_CLIENT_STUB(int, pong_subset)(struct usr_inv_cap *uc, word_t p0, word_t p1, word_t *r1)
{
	word_t r2;
	return cos_sinv_2rets(uc->cap_no, p0, p1, 0, 0, r1, &r2);
}

COS_CLIENT_STUB(word_t, pong_ids)(struct usr_inv_cap *uc, word_t *r1, word_t *r2)
{
	return cos_sinv_2rets(uc->cap_no, 0, 0, 0, 0, r1, r2);
}
