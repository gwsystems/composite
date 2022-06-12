#include <cos_types.h>
#include <memmgr.h>
#include <nicshmem.h>
#include <shm_bm.h>
#include <string.h>
#include <sched.h>
#include <nic.h>
#include <cos_dpdk.h>
#include "interface_impl.h"

typedef unsigned long cos_paddr_t; /* physical address */
typedef unsigned long cos_vaddr_t; /* virtual address */
extern cos_paddr_t cos_map_virt_to_phys(cos_vaddr_t addr);
extern char* g_tx_mp;
extern char* g_rx_mp;
struct pkt_data_buf *obj;

/* indexed by thread id */
struct client_session client_sessions[NIC_MAX_SESSION];

int
nic_shemem_map(cbuf_t shmid) {
	unsigned long npages;
	void         *mem;
	thdid_t thd;
	shm_bm_t shm;
	cos_paddr_t paddr = 0;

	npages = memmgr_shared_page_map_aligned(shmid, SHM_BM_ALIGN, (vaddr_t *)&mem);

	shm = shm_bm_create_data_buf(mem, npages * PAGE_SIZE);
	assert(shm);
	paddr = cos_map_virt_to_phys((cos_vaddr_t)shm);
	assert(paddr);

	thd = cos_thdid();
	assert(thd < NIC_MAX_SESSION);

	client_sessions[thd].shemem_info[0].shmid = shmid;
	client_sessions[thd].shemem_info[0].shm   = shm;
	client_sessions[thd].shemem_info[0].paddr = paddr;

	return 0;
}

shm_bm_objid_t
nic_get_a_packet(cbuf_t shmid)
{
	// sched_thd_block(0);
	cos_allocate_mbuf(g_tx_mp);
	printc("test g_tx_mp\n");


	return 0;
}

static void
ext_buf_free_callback_fn(void *addr, void *opaque)
{
	bool *freed = opaque;

	if (addr == NULL) {
		printc("External buffer address is invalid\n");
		return;
	}

	*freed = true;
	printc("External buffer freed via callback\n");
}

int
nic_send_packet(shm_bm_objid_t pktid, u16_t pkt_len)
{
	thdid_t thd;
	shm_bm_objid_t   objid;
	thd = cos_thdid();
	struct data_buffer *obj;

	objid = pktid;

	obj = (struct data_buffer*)shm_bm_take_data_buf(client_sessions[thd].shemem_info[0].shm, objid);

	char* mbuf = cos_allocate_mbuf(g_tx_mp);

	u64_t data_paddr = client_sessions[thd].shemem_info[0].paddr 
		+ (u64_t)&obj->data - (u64_t)client_sessions[thd].shemem_info[0].shm;

	cos_attach_external_mbuf(mbuf, &obj->data, PKT_BUF_SIZE, ext_buf_free_callback_fn, data_paddr);

	cos_send_external_packet(mbuf, pkt_len);

	cos_get_port_stats(0);

	return 0;
}

extern void tls_init();
int
nic_bind_port(u32_t ip_addr, u16_t port)
{
	thdid_t thd;
	thd = cos_thdid();
	assert(thd < NIC_MAX_SESSION);
	client_sessions[thd].ip_addr = ip_addr;
	client_sessions[thd].port    = port;

	tls_init();
	printc("tls init done\n");
	return 0;
}
