#include <cos_types.h>
#include <initargs.h>
#include <string.h>
#include <arpa/inet.h>
#include <netshmem.h>
#include <res_spec.h>
#include <sched.h>
#include <vmm_netio_rx.h>
#include <vmm_netio_tx.h>
#include <vmm_netio_shmem.h>
#include <nic_netio_rx.h>
#include <nic_netio_tx.h>
#include <nic_netio_shmem.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define NF_THD_PRIORITY 31

thdid_t rx_tid = 0;
thdid_t tx_tid = 0;

static u16_t nf_port = 0;
static u16_t nf_vmm = 0;
static char *nf_ip = 0;

char tx_nf_buffer[4096];
char rx_nf_buffer[4096];

#define RX_BATCH 1
#define TX_BATCH 1

#define RX_PROCESSING 1
#define TX_PROCESSING 1

enum {
	NF_ACCEPT,
	NF_DROP
};

static int
firewall(unsigned char *payload)
{
	struct iphdr *iphdr = (struct iphdr *)payload;

	struct firewall_rule
	{
		uint32_t ip;
		uint8_t action; // DROP or ACCEPT
	};

	struct firewall_rule rules[] = {
		{.ip = inet_addr("192.168.9.9"), .action = NF_DROP},
	};

	const int num_rules = sizeof(rules) / sizeof(rules[0]);

	u32_t src_ip = iphdr->saddr;

	for (int i = 0; i < num_rules; i++)
	{
		struct in_addr src_addr;
		src_addr.s_addr = src_ip;
		char *src_ip_str = inet_ntoa(src_addr);

		struct in_addr rule_addr;
		rule_addr.s_addr = rules[i].ip;
		char *rule_ip_str = inet_ntoa(rule_addr);

		if (src_ip == rules[i].ip)
		{
			return rules[i].action;
		}
	}

	return NF_ACCEPT;
}

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

	vmm_netio_shmem_map(netshmem_get_shm_id());
	nic_netio_shmem_map(netshmem_get_shm_id());

	ip = inet_addr(nf_ip);
	nic_netio_shmem_bind_port(ip, nf_port);

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
		objid = nic_netio_rx_packet(&pkt_len);
		vmm_netio_tx_packet(objid, pkt_len);
#else
		first_objid = nic_netio_rx_packet_batch(batch_ct);

		first_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, first_objid);
		first_obj_pri = netshmem_get_pri(first_obj);
		pkt_arr = (struct netshmem_meta_tuple *)&(first_obj_pri->pkt_arr);
		rx_batch_ct = first_obj_pri->batch_len;
#if RX_PROCESSING
		for (u8_t i = 0; i < rx_batch_ct; i++) {
			pkt_len = pkt_arr[i].pkt_len;
			objid = pkt_arr[i].obj_id;
			rx_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, objid);
			if (firewall(netshmem_get_data_buf(rx_obj)) == NF_DROP) {
				shm_bm_free_net_pkt_buf(rx_obj);
			}
		}
#endif

		vmm_netio_tx_packet_batch(first_objid);
#endif
	}
}

static void
tx_task(void)
{
	u16_t pkt_len;
	shm_bm_objid_t objid;

	vmm_netio_shmem_map(netshmem_get_shm_id());
	nic_netio_shmem_map(netshmem_get_shm_id());

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

	ip = inet_addr(nf_ip);
	nic_netio_shmem_bind_port(ip, nf_port + 1);

	int svc_id = nf_port;

	vmm_netio_shmem_svc_update(svc_id, 0);

	while(1) {
		u8_t batch_ct = 32;

		first_objid = objid = vmm_netio_rx_packet_batch(batch_ct);
		first_obj = shm_bm_transfer_net_pkt_buf(tx_shmemd, first_objid);
		first_obj_pri = netshmem_get_pri(first_obj);
		pkt_arr = (struct netshmem_meta_tuple *)&(first_obj_pri->pkt_arr);
		tx_batch_ct = first_obj_pri->batch_len;

#if TX_PROCESSING
		for (u8_t i = 0; i < tx_batch_ct; i++) {
			pkt_len = pkt_arr[i].pkt_len;
			objid = pkt_arr[i].obj_id;
			tx_obj = shm_bm_transfer_net_pkt_buf(tx_shmemd, objid);
			if (firewall(netshmem_get_data_buf(tx_obj)) == NF_DROP) {
				shm_bm_free_net_pkt_buf(tx_obj);
			}
		}
#endif

		nic_netio_tx_packet_batch(first_objid);
	}
}

void
cos_init(void)
{
	struct initargs params, curr;
	struct initargs_iter i;
	int ret = 0;

	ret = args_get_entry("param", &params);
	assert(!ret);

	for (ret = args_iter(&params, &i, &curr) ; ret ; ret = args_iter_next(&i, &curr)) {
		int      keylen;
		char *nf_key = args_key(&curr, &keylen);
		char *nf_val = args_value(&curr);
		if (!strcmp(nf_key, "ip")) {
			nf_ip = nf_val;
		} else if (!strcmp(nf_key, "port")) {
			sscanf(nf_val, "%u", &nf_port);
		} else if (!strcmp(nf_key, "vmm_id")) {
			sscanf(nf_val, "%u", &nf_vmm);
		}
	}
	printc("nf_ip:%s, nf_port:%d, nf_vmm:%u\n", nf_ip, nf_port, nf_vmm);
}


void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	if (cid == nf_vmm) {
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
	if (cid == nf_vmm) {
		sched_thd_param_set(rx_tid, sched_param_pack(SCHEDP_PRIO, NF_THD_PRIORITY));
		sched_thd_param_set(tx_tid, sched_param_pack(SCHEDP_PRIO, NF_THD_PRIORITY));
	}

	sched_thd_block(0);
	return 0;
}