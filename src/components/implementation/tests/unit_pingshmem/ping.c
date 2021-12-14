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

	printc("PING HELLO WORLD\n");

	id = memmgr_shared_page_allocn(1, &addr);
	memmgr_shared_page_map(id, &addr);

	PRINTLOG(PRINT_DEBUG, "%s: shared memory allocation in ping\n", (id == 0) ? "FAILURE" : "SUCCESS");
	printc("PTR: %p\n", addr);

	*((int *) addr) = 69;
	pongshmem_read(id);

	return 0;
}
