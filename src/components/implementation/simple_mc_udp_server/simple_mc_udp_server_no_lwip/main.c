#include "initargs.h"
#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <mc.h>
#include <simple_udp_stack.h>
#include <netshmem.h>
#include <sched.h>

#define MULTI_GET 2

static int fd;
static volatile thdid_t init_thd = 0;
static volatile int dst_core = -1;
int checker[NUM_CPU] = {0};

void
cos_init(void)
{
	struct initargs params, curr;
	struct initargs_iter i;

	int ret = args_get_entry("param", &params);
	assert(!ret);
	for (ret = args_iter(&params, &i, &curr); ret; ret = args_iter_next(&i, &curr)) {
		dst_core = atoi(args_value(&curr));
		//assert(dst_core > 0 && dst_core < NUM_CPU);
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

	printc("mc server init done, got a fd: %d, %d\n", fd, dst_core);
	printc("MULTI_GET: %d\n", MULTI_GET);
}


#define PORT_OFFSET  5

int
parallel_main(coreid_t cid)
{
	int ret;
	u32_t ip;
	//compid_t compid;
	u16_t port;
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *rx_obj;
	struct netshmem_pkt_buf *tx_obj;
	char *data;
	u16_t data_offset, data_len, rdysend_len;
	u16_t remote_port;
	u32_t remote_addr;
	char temp_data[PKT_BUF_SIZE];

	assert(dst_core >= 0);
	if (cos_cpuid() != dst_core) return 0;
	int c = ps_faa(&checker[cos_cpuid()], 1);
	assert(c == 0);

	ret = 0;
	ip = inet_addr("10.10.1.2");
	//compid = cos_compid();

	/* we use comp id as UDP port, representing tenant id */
	//assert(compid < (1 << 16));
	//port	= (u16_t)compid;

	port = dst_core + PORT_OFFSET;
	//printc("%x\n", port);
	ret = udp_stack_udp_bind(ip, port);
	assert(ret == 0);
	objid = 0;
	remote_addr = inet_addr("10.10.1.1");
	remote_port = 6;
	unsigned long s, e = 0;

	while (1)
	{
		objid  = udp_stack_shmem_read(&data_offset, &data_len, &remote_addr, &remote_port);
		/* application would like to own the shmem because it does not want ohters to free it. */
		rx_obj = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
		if (unlikely(data_len == 0)) {
			// invalid packet, drop it
			assert(0);
			shm_bm_free_net_pkt_buf(rx_obj);
			continue;
		}
		if (MULTI_GET > 1) memcpy(temp_data, rx_obj, data_len);
		cycles_t start = ps_tsc();
		for (int i = 0; i < MULTI_GET; i++) {
			rdysend_len = mc_process_command(fd, objid, data_offset, data_len);
			assert(rdysend_len);
			if (i < MULTI_GET-1) memcpy(rx_obj, temp_data, data_len);
		}
		cycles_t end = ps_tsc();
		//if (cos_cpuid() == 1) printc("%llu\n", end-start);

		udp_stack_shmem_write(objid, netshmem_get_data_offset(), rdysend_len, remote_addr, remote_port);
		/*if (cos_cpuid() == 1) {
			mc_print();
		}*/
	}
}
