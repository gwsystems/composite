#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netshmem.h>
#include <res_spec.h>
#include <sched.h>
#include <netio.h>
#include <nic.h>

#define TX_PRIORITY 30
thdid_t rx_tid = 0;
thdid_t tx_tid = 0;

static void
tx_task(void)
{
	u16_t pkt_len;
	shm_bm_objid_t objid;

	netio_shmem_map(netshmem_get_shm_id());
	nic_shmem_map(netshmem_get_shm_id());

	nic_bind_port(0, 1);
	while(1) {
		objid = netio_get_a_packet(&pkt_len);
		nic_send_packet(objid, 0, pkt_len);
	}
}

void
cos_init(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;

	/* create current component's shmem */
	rx_tid = cos_thdid();
	netshmem_create(rx_tid);
	tx_tid = sched_thd_create((void *)tx_task, NULL);
	netshmem_create(tx_tid);

	printc("NF application init shm done\n");
}

int
parallel_main(coreid_t cid)
{
	/* TODO: port NF applications here   */
	shm_bm_objid_t           objid;
	u16_t pkt_len;

	sched_thd_param_set(tx_tid, sched_param_pack(SCHEDP_PRIO, TX_PRIORITY));

	netio_shmem_map(netshmem_get_shm_id());
	nic_shmem_map(netshmem_get_shm_id());
	nic_bind_port(0, 0);

	while (1)
	{
		objid = nic_get_a_packet(&pkt_len);
		netio_send_packet(objid, pkt_len);
	}	
}
