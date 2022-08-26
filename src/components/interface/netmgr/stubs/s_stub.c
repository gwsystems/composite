#include <cos_stubs.h>
#include <netmgr.h>

COS_SERVER_3RET_STUB(shm_bm_objid_t, netmgr_tcp_shmem_read)
{
	return netmgr_tcp_shmem_read(r1, r2);
}

COS_SERVER_3RET_STUB(shm_bm_objid_t, netmgr_udp_shmem_read)
{
	return netmgr_udp_shmem_read(r1, r2);
}
