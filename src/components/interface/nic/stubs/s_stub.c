#include <cos_stubs.h>
#include <nic.h>

COS_SERVER_3RET_STUB(shm_bm_objid_t, nic_get_a_packet)
{
	return nic_get_a_packet((u16_t *)r1);
}
