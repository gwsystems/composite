#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netshmem.h>
#include <simple_udp_stack.h>

static volatile thdid_t init_thd = 0;
volatile int initdone = 0;
static int initcore = 0;

struct conn_addr {
	u32_t ip;
	u16_t port;
};

void
cos_init(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;

	init_thd = cos_thdid();
	initcore = cos_cpuid();
	/* create current component's shmem */
	netshmem_create();
	//udp_stack_shmem_map(netshmem_get_shm_id());
	printc("init core init shm done\n");
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	printc("parallel init\n");
	if (init_thd != cos_thdid()) {
		netshemem_move(init_thd, cos_thdid());
	}
	udp_stack_shmem_map(netshmem_get_shm_id());
	assert(netshmem_get_shm());
	if (cos_faa(&initdone, 1) == (NUM_CPU-1) && cos_cpuid() == init_core) {
		printc("Init done\n");
	}
}

int
parallel_main(coreid_t cid)
{
	int ret;
	u32_t ip;
	compid_t compid;
	u16_t port;
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *rx_obj;
	struct netshmem_pkt_buf *tx_obj;
	char *data;
	u16_t data_offset, data_len;
	u16_t remote_port;
	u32_t remote_addr;

	ret = 0;
	ip = inet_addr("10.10.1.2");
	compid = cos_compid();

	/* we use comp id as UDP port, representing tenant id */
	assert(compid < (1 << 16));
	port	= (u16_t)compid;

	//printc("tenant id:%d\n", port);
	ret = udp_stack_udp_bind(ip, port);
	assert(ret == 0);

	while (1)
	{
		objid  = udp_stack_shmem_read(&data_offset, &data_len, &remote_addr, &remote_port);
		/* application would like to own the shmem because it does not want ohters to free it. */
		rx_obj = shm_bm_transfer_net_pkt_buf(netshmem_get_shm(), objid);
		data = rx_obj->data + data_offset;

		tx_obj = shm_bm_alloc_net_pkt_buf(netshmem_get_shm(), &objid);
		assert(tx_obj);
		memcpy(netshmem_get_data_buf(tx_obj), data, data_len);

		/* application free unused rx buf */
		shm_bm_free_net_pkt_buf(rx_obj);

		udp_stack_shmem_write(objid, netshmem_get_data_offset(), data_len, remote_addr, remote_port);
		shm_bm_free_net_pkt_buf(tx_obj);
	}
}
