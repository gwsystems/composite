#include <cos_component.h>
#include <cos_stubs.h>
#include <netmgr.h>


COS_CLIENT_STUB(shm_bm_objid_t, netmgr_tcp_shmem_read, u16_t *data_offset, u16_t *data_len)
{
	COS_CLIENT_INVCAP;
	word_t offset, len;
	int ret;

	ret = cos_sinv_2rets(uc->cap_no, 0, 0, 0, 0, &offset, &len);
	*data_len = (u16_t)len;
	*data_offset = (u16_t)offset;

	return ret;
}

COS_CLIENT_STUB(shm_bm_objid_t, netmgr_udp_shmem_read, u16_t *data_offset, u16_t *data_len)
{
	COS_CLIENT_INVCAP;
	word_t offset, len;
	int ret;

	ret = cos_sinv_2rets(uc->cap_no, 0, 0, 0, 0, &offset, &len);
	*data_len = (u16_t)len;
	*data_offset = (u16_t)offset;

	return ret;
}
