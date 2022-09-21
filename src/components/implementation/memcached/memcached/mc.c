
#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netmgr.h>
#include <netshmem.h>
#include <mc.h>
#include <cos_memcached.h>

void
mc_map_shmem(cbuf_t shm_id)
{
	netshmem_map_shmem(shm_id);
}

int
mc_conn_init(int proto)
{
	return cos_mc_new_conn(proto);
}

u16_t
mc_process_command(int fd, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len)
{
	shm_bm_t shm = netshmem_get_shm();
	struct netshmem_pkt_buf *pkt_buf = shm_bm_borrow_net_pkt_buf(shm, objid);
	char *r_buf = (char *)pkt_buf + data_offset;
	char *w_buf = netshmem_get_data_buf(pkt_buf);

	/* after this call, memcached should have data written into w_buf */
	return cos_mc_process_command(fd, r_buf, data_len, w_buf, netshmem_get_max_data_buf_sz());
}

void
cos_init(void)
{
	int argc, ret;

	char *argv[] =	{
		"--listen=10.10.2.2",
		"--port=0",// close tcp initialization
		"--udp-port=11211",
		"--threads=64",
		"--protocol=auto",
		"--memory-limit=64",
		"--extended=no_lru_crawler,no_lru_maintainer,no_hashexpand,no_slab_reassign",
	};

	argc = ARRAY_SIZE(argv);

	/* 1. do initialization of memcached */
	ret = cos_mc_init(argc, argv);
	printc("memcached init done, ret: %d\n", ret);
}
