#include <cos_component.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(int, pong_wideargs, long long p0, long long p1)
{
	COS_CLIENT_INVCAP;
#if defined(__WORD_SIZE_64__)
	return cos_sinv(uc, p0, p1, 0, 0);
#else
	long p0h, p0l, p1h, p1l;

	COS_ARG_DWORD_TO_WORD(p0, p0h, p0l);
	COS_ARG_DWORD_TO_WORD(p1, p1h, p1l);


	return cos_sinv(uc, p0h, p0l, p1h, p1l);
#endif
}

COS_CLIENT_STUB(int, pong_argsrets, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)
{
	COS_CLIENT_INVCAP;

	return cos_sinv_2rets(uc, p0, p1, p2, p3, r1, r2);
}

COS_CLIENT_STUB(long long, pong_widerets, long long p0, long long p1)
{
	COS_CLIENT_INVCAP;
	word_t r1 = 0, r2 = 0;
	long long ret = 0;
	long p0h, p0l, p1h, p1l;
	
	COS_ARG_DWORD_TO_WORD(p0, p0h, p0l);
	COS_ARG_DWORD_TO_WORD(p1, p1h, p1l);

	cos_sinv_2rets(uc, p0h, p0l, p1h, p1l, &r1, &r2);
	ret = ((long long)r1 << 32) | (long long)r2;

	return ret;
}

COS_CLIENT_STUB(int, pong_subset, word_t p0, word_t p1, word_t *r1)
{
	COS_CLIENT_INVCAP;
	word_t r2;

	return cos_sinv_2rets(uc, p0, p1, 0, 0, r1, &r2);
}

COS_CLIENT_STUB(word_t, pong_ids, word_t *r1, word_t *r2)
{
	COS_CLIENT_INVCAP;

	return cos_sinv_2rets(uc, 0, 0, 0, 0, r1, r2);
}
