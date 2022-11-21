#include <cos_component.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(int, syncipc_call, int ipc_ep, word_t a0, word_t a1, word_t *r0, word_t *r1)
{
	COS_CLIENT_INVCAP;

	return cos_sinv_2rets(uc->cap_no, ipc_ep, a0, a1, 0, r0, r1);
}

COS_CLIENT_STUB(int, syncipc_reply_wait, int ipc_ep, word_t a0, word_t a1, word_t *r0, word_t *r1)
{
	COS_CLIENT_INVCAP;

	return cos_sinv_2rets(uc->cap_no, ipc_ep, a0, a1, 0, r0, r1);
}
