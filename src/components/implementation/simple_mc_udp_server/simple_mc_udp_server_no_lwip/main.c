#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <mc.h>
#include <simple_udp_stack.h>
#include <netshmem.h>
#include <sched.h>

static int fd;
static volatile thdid_t init_thd = 0;

void
cos_init(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;
	init_thd = cos_thdid();
	/* create current component's shmem */
	netshmem_create();
}
void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	if (cid == 0) {
		return;
	}

	if (cos_compid() == 6 && cid != 1) {
		return;
	}
	if (cos_compid() == 7 && cid != 2) {
		return;
	}
	if (cos_compid() == 8 && cid != 3) {
		return;
	}
	if (cos_compid() == 9 && cid != 4) {
		return;
	}
	if (cos_compid() == 10 && cid != 5) {
		return;
	}
	if (cos_compid() == 11 && cid != 6) {
		return;
	}
	if (cos_compid() == 12 && cid != 7) {
		return;
	}
	if (cos_compid() == 13 && cid != 8) {
		return;
	}
	if (init_thd != cos_thdid()) {
		netshemem_move(init_thd, cos_thdid());
	}
	udp_stack_shmem_map(netshmem_get_shm_id());
	mc_map_shmem(netshmem_get_shm_id());
	fd = mc_conn_init(MC_UDP_PROTO);
	assert(fd);

	printc("mc server init done, got a fd: %d\n", fd);
}

int
parallel_main(coreid_t cid)
{
	if (cid == 0) {
		return 0;
	}
	if (cos_compid() == 6 && cid != 1) {
		return 0;
	}
	if (cos_compid() == 7 && cid != 2) {
		return 0;
	}
	if (cos_compid() == 8 && cid != 3) {
		return 0;
	}
	if (cos_compid() == 9 && cid != 4) {
		return 0;
	}
	if (cos_compid() == 10 && cid != 5) {
		return 0;
	}
	if (cos_compid() == 11 && cid != 6) {
		return 0;
	}
	if (cos_compid() == 12 && cid != 7) {
		return 0;
	}
	if (cos_compid() == 13 && cid != 8) {
		return 0;
	}
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

	printc("tenant id:%d\n", port);
	ret = udp_stack_udp_bind(ip, port);
	assert(ret == 0);
	objid = 0;
	remote_addr = inet_addr("10.10.1.1");
	remote_port = 6;
	cycles_t    before, after;
	
	while (1)
	{
		// rdtscll(before);
		objid  = udp_stack_shmem_read(&data_offset, &data_len, &remote_addr, &remote_port);
		// rdtscll(after);
		// printc("gap:%lu\n", after - before);
		// printc("rx a objid:%u\n", objid);

		/* application would like to own the shmem because it does not want ohters to free it. */
		rx_obj = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
		if (unlikely(data_len == 0)) {
			//invalid packet, drop it
			shm_bm_free_net_pkt_buf(rx_obj);
			continue;
		}
		// rdtscll(before);
		data_len = mc_process_command(fd, objid, data_offset, data_len);
		// rdtscll(after);
		// if (cos_compid() == 6) {
			// printc("gap:%lu\n", after - before);
		// }
		// printc("???\n");
		// rx_obj = shm_bm_alloc_net_pkt_buf(netshmem_get_shm(), &objid);
		// data_len = 100 + cos_compid();
		// rdtscll(before);
		udp_stack_shmem_write(objid, netshmem_get_data_offset(), data_len, remote_addr, remote_port);
		// rdtscll(after);
		// printc("gap:%lu\n", after - before);
	}
}
