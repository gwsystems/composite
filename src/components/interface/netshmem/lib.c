#include "netshmem.h"

struct netshmem netshmems[NETSHMEM_REGION_SZ];

void
netshmem_init(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;
	void  *mem;

	/* init rx shmem */
	netshmems[NETSHMEM_RX].shmsz	= round_up_to_page(shm_bm_size_rx_pkt_buf());
	netshmems[NETSHMEM_RX].shm_id	= contigmem_shared_alloc_aligned(netshmems[NETSHMEM_RX].shmsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);
	netshmems[NETSHMEM_RX].shm	= shm_bm_create_rx_pkt_buf(mem, netshmems[NETSHMEM_RX].shmsz);

	shm_bm_init_rx_pkt_buf(netshmems[NETSHMEM_RX].shm);

	/* init tx shmem */
	netshmems[NETSHMEM_TX].shmsz	= round_up_to_page(shm_bm_size_tx_pkt_buf());
	netshmems[NETSHMEM_TX].shm_id	= contigmem_shared_alloc_aligned(netshmems[NETSHMEM_TX].shmsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);
	netshmems[NETSHMEM_TX].shm	= shm_bm_create_rx_pkt_buf(mem, netshmems[NETSHMEM_TX].shmsz);

	shm_bm_init_tx_pkt_buf(netshmems[NETSHMEM_TX].shm);
}

cbuf_t
netshmem_get_rx_shm_id(void)
{
	return netshmems[NETSHMEM_RX].shm_id;
}

cbuf_t
netshmem_get_tx_shm_id(void)
{
	return netshmems[NETSHMEM_TX].shm_id;
}

shm_bm_t
netshmem_get_rx_shm(void)
{
	return netshmems[NETSHMEM_RX].shm;
}

shm_bm_t
netshmem_get_tx_shm(void)
{
	return netshmems[NETSHMEM_TX].shm;
}

void
netshmem_map_shmem(cbuf_t rx_shm_id, cbuf_t tx_shm_id)
{
	unsigned long npages;
	void         *mem;
	shm_bm_t shm;

	npages	= memmgr_shared_page_map_aligned(rx_shm_id, SHM_BM_ALIGN, (vaddr_t *)&mem);
	// printc("npages:%u\n",npages);
	shm	= shm_bm_create_rx_pkt_buf(mem, npages * PAGE_SIZE);
	assert(shm);

	netshmems[NETSHMEM_RX].shm = shm;
	netshmems[NETSHMEM_RX].shm_id = rx_shm_id;
	netshmems[NETSHMEM_RX].shmsz = npages * PAGE_SIZE;

	npages	= memmgr_shared_page_map_aligned(tx_shm_id, SHM_BM_ALIGN, (vaddr_t *)&mem);
	shm	= shm_bm_create_tx_pkt_buf(mem, npages * PAGE_SIZE);

	assert(shm);

	netshmems[NETSHMEM_TX].shm = shm;
	netshmems[NETSHMEM_TX].shm_id = tx_shm_id;
	netshmems[NETSHMEM_TX].shmsz = npages * PAGE_SIZE;
}
