#include "initargs.h"
#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <mc.h>
#include <simple_udp_stack.h>
#include <netshmem.h>
#include <sched.h>

static int fd;
static volatile thdid_t init_thd = 0;
static volatile int dst_core = -1;

void
cos_init(void)
{
	struct initargs params, curr;
	struct initargs_iter i;

	int ret = args_get_entry("param", &params);
	assert(!ret);
	for (ret = args_iter(&params, &i, &curr); ret; ret = args_iter_next(&i, &curr)) {
		dst_core = atoi(args_value(&curr));
	//	printc("dst: %d\n", dst_core);
	}

	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;
	init_thd = cos_thdid();
	/* create current component's shmem */
	netshmem_create();
}
void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	assert(dst_core >= 0);
	if (cos_cpuid() != dst_core) return;

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

	assert(dst_core >= 0);
	if (cos_cpuid() != dst_core)  return 0;

	ret = 0;
	ip = inet_addr("10.10.1.2");
	compid = cos_compid();

	/* we use comp id as UDP port, representing tenant id */
	assert(compid < (1 << 16));
	port	= (u16_t)compid;

	printc("%x\n", port);
	ret = udp_stack_udp_bind(ip, port);
	assert(ret == 0);
	objid = 0;
	remote_addr = inet_addr("10.10.1.1");
	remote_port = 6;
	cycles_t    before, after, tot;
	int cnt = 0 ;
	
	while (1)
	{
		before = ps_tsc();
		objid  = udp_stack_shmem_read(&data_offset, &data_len, &remote_addr, &remote_port);
		/* application would like to own the shmem because it does not want ohters to free it. */
		rx_obj = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
		if (unlikely(data_len == 0)) {
			// invalid packet, drop it
			shm_bm_free_net_pkt_buf(rx_obj);
			continue;
		}
		data_len = mc_process_command(fd, objid, data_offset, data_len);
		udp_stack_shmem_write(objid, netshmem_get_data_offset(), data_len, remote_addr, remote_port);
		after = ps_tsc();
		tot += (after-before);
		cnt++;
		if (cnt > 100000 && cos_cpuid() == 1) {
			//printc("%llu\n", tot/cnt);
			tot = 0;
			cnt = 0;
		}
	}
}
