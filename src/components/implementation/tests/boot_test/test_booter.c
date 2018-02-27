#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>
#include <hypercall.h>

void
cos_init(void)
{
	PRINTC("Dummy component booted.\n");

	hypercall_comp_init_done();

	while (1) ;
}
