#include <cos_stubs.h>
#include <netmgr.h>

COS_SERVER_3RET_STUB(shm_bm_objid_t, netmgr_tcp_shmem_read)
{
	return netmgr_tcp_shmem_read((u16_t *)r1, (u16_t *)r2);
}

COS_SERVER_3RET_STUB(shm_bm_objid_t, netmgr_udp_shmem_read)
{
	u16_t data_offset, data_len, remote_port;
	u32_t remote_addr;
	shm_bm_objid_t objid;

	objid = netmgr_udp_shmem_read(&data_offset, &data_len, &remote_addr, &remote_port);

	*r1 = (word_t)(data_offset) << 32 | data_len;
	*r2 = (word_t)(remote_addr) << 32 | remote_port;

	return objid;
}

COS_SERVER_3RET_STUB(shm_bm_objid_t, netmgr_udp_shmem_write)
{
	return netmgr_udp_shmem_write(p0, p1, p2, (p3 >> 32), p3);
}
