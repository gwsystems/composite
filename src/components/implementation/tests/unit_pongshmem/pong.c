#include <cos_types.h>
#include <memmgr.h>
#include <pongshmem.h>


void
pongshmem_read(cbuf_t id)
{
	vaddr_t addr;
	int failure; 

	memmgr_shared_page_map(id, &addr);
	
	PRINTLOG(PRINT_DEBUG, "%s: shared memory mapped in pong\n", (addr == 0) ? "FAILURE" : "SUCCESS");

	failure = strcmp((char *) addr, "PINGPING") != 0;
	PRINTLOG(PRINT_DEBUG, "%s: pong can read data from ping\n", (failure) ? "FAILURE" : "SUCCESS");

	strcpy((char *) addr, "PONGPONG");
	
}
