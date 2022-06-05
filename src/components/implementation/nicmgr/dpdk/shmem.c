#include <cos_types.h>
#include <memmgr.h>
#include <nicshmem.h>
#include <shm_bm.h>
#include <string.h>

shm_bm_t shm;
struct pkt_data_buf *obj;

void 
nicshmem_test_map(cbuf_t shmid)
{
	unsigned long npages;
	void         *mem;

	npages = memmgr_shared_page_map_aligned(shmid, SHM_BM_ALIGN, (vaddr_t *)&mem);

	shm = shm_bm_create_shemem_data_buf(mem, npages * PAGE_SIZE);
}

void
nicshmem_test_objread(shm_bm_objid_t objid, int test_string)
{
	
	int              failure; 

	obj = (struct pkt_data_buf *)shm_bm_take_shemem_data_buf(shm, objid);
}
