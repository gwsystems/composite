
#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netmgr.h>
#include <netshmem.h>

cbuf_t rx_shm_id;
cbuf_t tx_shm_id;

size_t rx_shmsz;
size_t tx_shmsz;

shm_bm_t rx_shm;
shm_bm_t tx_shm;

void
cos_init(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;
	netshmem_init();

	netmgr_shmem_init(netshmem_get_rx_shm_id(), netshmem_get_tx_shm_id());

	printc("app init shm done\n");
}

int
main(void)
{
	int ret		= 0;
	u32_t ip	= inet_addr("10.10.1.2");
	u16_t port	= 80;
	struct conn_addr client_addr;
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *rx_obj;
	struct netshmem_pkt_buf *tx_obj;
	char *data;

	ret = netmgr_tcp_bind(ip, port);
	assert(ret == NETMGR_OK);

	ret = netmgr_tcp_listen(16);
	assert(ret == NETMGR_OK);

	printc("App begin to accept connection\n");
	ret = netmgr_tcp_accept(&client_addr);
	assert(ret == NETMGR_OK);

	while (1)
	{
		/* code */
		objid = netmgr_tcp_shmem_read();
		rx_obj = shm_bm_take_rx_pkt_buf(netshmem_get_rx_shm(), objid);
		
		data = rx_obj->data + rx_obj->payload_offset;

		tx_obj = shm_bm_alloc_tx_pkt_buf(netshmem_get_tx_shm(), &objid);
		memcpy(tx_obj->data + NETSHMEM_HEADROOM, data, rx_obj->payload_sz);
		tx_obj->payload_offset = NETSHMEM_HEADROOM;
		tx_obj->payload_sz = rx_obj->payload_sz;

		netmgr_tcp_shmem_write(objid);
	}
	

}
