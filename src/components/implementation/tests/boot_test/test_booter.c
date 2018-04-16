#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>
#include <hypercall.h>

void
cos_init(void)
{
	PRINTLOG(PRINT_DEBUG, "Dummy component booted.\n");

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
