#include <cos_component.h>
#include <contigmem.h>
#include "netshmem.h"

struct netshmem netshmems[NETSHMEM_REGION_SZ] = {0};

void
netshmem_create(void)
{
	shm_bm_objid_t objid;
	struct netshmem_pkt_buf *obj;
	void  *mem;

	thdid_t thd = cos_thdid();
	assert(thd < NETSHMEM_REGION_SZ);

	/* init rx shmem */
	netshmems[thd].shmsz	= round_up_to_page(shm_bm_size_net_pkt_buf());
	netshmems[thd].shm_id	= contigmem_shared_alloc_aligned(netshmems[thd].shmsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);
	netshmems[thd].shm	= shm_bm_create_net_pkt_buf(mem, netshmems[thd].shmsz);

	/* shmem cannot be NULL */
	assert(netshmems[thd].shm);

	shm_bm_init_net_pkt_buf(netshmems[thd].shm);
}

cbuf_t
netshmem_get_shm_id()
{
	return netshmems[cos_thdid()].shm_id;
}

shm_bm_t
netshmem_get_shm()
{
	return netshmems[cos_thdid()].shm;
}

void
netshmem_map_shmem(cbuf_t shm_id)
{
	unsigned long npages;
	void         *mem;
	shm_bm_t      shm;

	thdid_t thd = cos_thdid();

	assert(!netshmems[thd].shm);

	npages	= memmgr_shared_page_map_aligned(shm_id, SHM_BM_ALIGN, (vaddr_t *)&mem);
	shm	= shm_bm_create_net_pkt_buf(mem, npages * PAGE_SIZE);
	assert(shm);

	netshmems[thd].shm	= shm;
	netshmems[thd].shm_id	= shm_id;
	netshmems[thd].shmsz	= npages * PAGE_SIZE;
}
