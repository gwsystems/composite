#include <cos_stubs.h>
#include <nic_netio_rx.h>

COS_SERVER_3RET_STUB(shm_bm_objid_t, nic_netio_rx_packet)
{
	return nic_netio_rx_packet((u16_t *)r1);
}
