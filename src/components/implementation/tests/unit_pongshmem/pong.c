#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ps.h>
#include <capmgr.h>
#include <memmgr.h>
#include <pongshmem.h>

int
main(void)
{
	printc("PONG HELLO WORLD\n");

	return 0;
}

void
pongshmem_read(cbuf_t id)
{
	vaddr_t addr;

	printc("READING....\n");

	memmgr_shared_page_map(id, &addr);
	
	PRINTLOG(PRINT_DEBUG, "%s: shared memory mapped in pong\n", (addr == 0) ? "FAILURE" : "SUCCESS");


	printc("MEM: %d\n", *((int *) addr));
}
