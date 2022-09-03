
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

	/* create current component's shmem */
	netshmem_create();

	netmgr_shmem_map(netshmem_get_shm_id());

	printc("app init shm done\n");
}

#if 0
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
	u16_t data_offset, data_len;

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
		objid  = netmgr_tcp_shmem_read(&data_offset, &data_len);
		rx_obj = shm_bm_take_net_pkt_buf(netshmem_get_shm(), objid);
		
		data = rx_obj->data + data_offset;

		tx_obj = shm_bm_alloc_net_pkt_buf(netshmem_get_shm(), &objid);
		memcpy(netshmem_get_data_buf(tx_obj), data, data_len);

		/* application free unused rx buf */
		shm_bm_free_net_pkt_buf(rx_obj);

		netmgr_tcp_shmem_write(objid, netshmem_get_data_offset(), data_len);
	}
	

}
#endif

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
	u16_t data_offset, data_len;

	ret = netmgr_udp_bind(ip, port);
	assert(ret == NETMGR_OK);

	while (1)
	{
		/* code */
		objid  = netmgr_udp_shmem_read(&data_offset, &data_len);
		/* application would like to own the shmem because it does not want ohters to free it. */
		rx_obj = shm_bm_transfer_net_pkt_buf(netshmem_get_shm(), objid);
		data = rx_obj->data + data_offset;

		tx_obj = shm_bm_alloc_net_pkt_buf(netshmem_get_shm(), &objid);
		assert(tx_obj);
		memcpy(netshmem_get_data_buf(tx_obj), data, data_len);

		/* application free unused rx buf */
		shm_bm_free_net_pkt_buf(rx_obj);

		netmgr_udp_shmem_write(objid, netshmem_get_data_offset(), data_len);
		shm_bm_free_net_pkt_buf(tx_obj);
	}
}
