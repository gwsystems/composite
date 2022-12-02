#include <evt.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(int, __evt_get, evt_id_t id, evt_wait_flags_t flags, evt_res_type_t *src, evt_res_data_t *ret_data)
{
	COS_CLIENT_INVCAP;
	word_t s = 0, d = 0;
	int ret;

	ret = cos_sinv_2rets(uc, id, flags, 0, 0, &s, &d);
	*src = s;
	*ret_data = d;

	return ret;
}
