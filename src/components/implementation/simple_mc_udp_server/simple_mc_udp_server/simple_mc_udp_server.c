#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netmgr.h>
#include <netshmem.h>
#include <mc.h>

static int fd;

void
cos_init(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;

	/* create current component's shmem */
	netshmem_create();

	netmgr_shmem_map(netshmem_get_shm_id());
	mc_map_shmem(netshmem_get_shm_id());

	fd = mc_conn_init(UDP_PROTO);
	assert(fd);

	printc("mc server init done, got a fd: %d\n", fd);
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
	u16_t data_offset, data_len;
	u16_t remote_port;
	u32_t remote_addr;

	ret = netmgr_udp_bind(ip, port);
	assert(ret == NETMGR_OK);

	while (1)
	{
		objid  = netmgr_udp_shmem_read(&data_offset, &data_len, &remote_addr, &remote_port);

		/* application would like to own the shmem because it does not want ohters to free it. */
		rx_obj = shm_bm_transfer_net_pkt_buf(netshmem_get_shm(), objid);

		data_len = mc_process_command(fd, objid, data_offset, data_len);
		netmgr_udp_shmem_write(objid, netshmem_get_data_offset(), data_len, remote_addr, remote_port);

		shm_bm_free_net_pkt_buf(rx_obj);
	}
}
