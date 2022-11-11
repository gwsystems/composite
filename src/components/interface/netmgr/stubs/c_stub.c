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

COS_CLIENT_STUB(shm_bm_objid_t, netmgr_udp_shmem_read, u16_t *data_offset, u16_t *data_len, u32_t *remote_addr, u16_t *remote_port)
{
	COS_CLIENT_INVCAP;
	word_t r1, r2;
	int ret;

	ret = cos_sinv_2rets(uc->cap_no, 0, 0, 0, 0, &r1, &r2);
	*data_len = (u16_t)r1;
	*data_offset = (u16_t)(r1 >> 32);
	*remote_port = (u16_t)(r2);
	*remote_addr = (u32_t)(r2 >> 32);

	return ret;
}

COS_CLIENT_STUB(int, netmgr_udp_shmem_write, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len, u32_t remote_ip, u16_t remote_port)
{
	COS_CLIENT_INVCAP;
	word_t r1, r2;
	int ret;

	ret = cos_sinv_2rets(uc->cap_no, objid, data_offset, data_len, (word_t)(remote_ip) << 32 | remote_port, &r1, &r2);

	return ret;
}
