#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <netshmem.h>
#include <res_spec.h>
#include <sched.h>
#include <netio.h>
#include <nic.h>

#define NF_THD_PRIORITY 31

thdid_t rx_tid = 0;
thdid_t tx_tid = 0;

char tx_nf_buffer[4096];
char rx_nf_buffer[4096];

#define RX_BATCH 1
#define TX_BATCH 1

#define RX_PROCESSING 1
#define TX_PROCESSING 0

extern int packet_handler(const char *pkt, int len);

static void
rx_task(void)
{
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *rx_obj;
	shm_bm_objid_t           first_objid;
	struct netshmem_pkt_buf   *first_obj;
	struct netshmem_pkt_pri   *first_obj_pri;
	struct netshmem_meta_tuple *pkt_arr;
	u16_t pkt_len;
	u32_t ip;
	
	u8_t batch_ct = 50;

	netio_shmem_map(netshmem_get_shm_id());
	nic_shmem_map(netshmem_get_shm_id());

	ip = inet_addr("10.10.1.2");
	nic_bind_port(ip, 0);

	int i = 0;
	u64_t times = 0;
	u64_t overhead = 0;

	shm_bm_t rx_shmemd = 0;

	rx_shmemd = netshmem_get_shm();
	assert(rx_shmemd);

	while(1)
	{
		u8_t rx_batch_ct = 0;
#if !RX_BATCH
		objid = nic_get_a_packet(&pkt_len);
		netio_send_packet(objid, pkt_len);
#else
		first_objid = nic_get_a_packet_batch(batch_ct);

		first_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, first_objid);
		first_obj_pri = netshmem_get_pri(first_obj);
		pkt_arr = (struct netshmem_meta_tuple *)&(first_obj_pri->pkt_arr);
		rx_batch_ct = first_obj_pri->batch_len;
#if RX_PROCESSING
		for (u8_t i = 0; i < rx_batch_ct; i++) {
			pkt_len = pkt_arr[i].pkt_len;
			objid = pkt_arr[i].obj_id;
			rx_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, objid);
			// memcpy(rx_nf_buffer, netshmem_get_data_buf(rx_obj), pkt_len);
			packet_handler(netshmem_get_data_buf(rx_obj), pkt_len);
		}
#endif

		netio_send_packet_batch(first_objid);
#endif
	}
}

static void
tx_task(void)
{
	u16_t pkt_len;
	shm_bm_objid_t objid;

	netio_shmem_map(netshmem_get_shm_id());
	nic_shmem_map(netshmem_get_shm_id());

	shm_bm_objid_t           first_objid;
	struct netshmem_pkt_buf   *first_obj;
	struct netshmem_pkt_pri   *first_obj_pri;
	struct netshmem_meta_tuple *pkt_arr;
	u8_t tx_batch_ct = 0;
	struct netshmem_pkt_buf *tx_obj;
	u32_t ip;	

	shm_bm_t tx_shmemd = 0;
	tx_shmemd = netshmem_get_shm();
	assert(tx_shmemd);

	ip = inet_addr("10.10.1.2");
	nic_bind_port(ip, 71);

	int svc_id = 0;

	netio_svc_update(svc_id, 0);

	while(1) {
		u8_t batch_ct = 32;

		first_objid = objid = netio_get_a_packet_batch(batch_ct);

		nic_send_packet_batch(first_objid);
	}
}

void
cos_init(void)
{
	void setup_ndpi(void);

	setup_ndpi();
	printc("ndpi set up !\n");
}


void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	if (cid == 0) {
		rx_tid = sched_thd_create((void *)rx_task, NULL);
		netshmem_create(rx_tid);
		tx_tid = sched_thd_create((void *)tx_task, NULL);
		netshmem_create(tx_tid);
		printc("NF rx tid:%ld, tx tid:%ld\n", rx_tid, tx_tid);
	}
}

int
parallel_main(coreid_t cid)
{
	if (cid == 0) {
		sched_thd_param_set(rx_tid, sched_param_pack(SCHEDP_PRIO, NF_THD_PRIORITY));
		sched_thd_param_set(tx_tid, sched_param_pack(SCHEDP_PRIO, NF_THD_PRIORITY));
	} else {

	}


	sched_thd_block(0);
	return 0;
}
