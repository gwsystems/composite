#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <capmgr.h>
#include <memmgr.h>
#include <pongshmem.h>

#include <string.h>

int
main(void)
{
	cbuf_t  id;
	vaddr_t addr;
	int failure;

	id = memmgr_shared_page_allocn(1, &addr);

	PRINTLOG(PRINT_DEBUG, "%s: shared memory allocation in ping\n", (id == 0) ? "FAILURE" : "SUCCESS");

	strcpy((char *) addr, "PINGPING");
	pongshmem_read(id);
	
	failure = strcmp((char *) addr, "PONGPONG") != 0;
	PRINTLOG(PRINT_DEBUG, "%s: ping can read data from pong\n", (failure) ? "FAILURE" : "SUCCESS");

	return 0;
}
