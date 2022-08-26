#include <cos_component.h>
#include <cos_stubs.h>
#include <nic.h>


COS_CLIENT_STUB(shm_bm_objid_t, nic_get_a_packet, u16_t *pkt_len)
{
	COS_CLIENT_INVCAP;
	word_t unused, addrret;
	int ret;

	ret = cos_sinv_2rets(uc->cap_no, 0, 0, 0, 0, &addrret, &unused);
	*pkt_len = (u16_t)addrret;

	return ret;
}
