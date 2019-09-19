#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>
#include <bitmap.h>
#include <schedinit.h>

void
cos_init(void)
{
	PRINTC("Spin Init!\n");
	schedinit_child();

	while (1) ;

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
