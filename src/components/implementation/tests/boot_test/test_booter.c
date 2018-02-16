#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>
#include <llboot.h>

void
cos_init(void)
{
	prints("\n|*****************************|\n");
	prints(" Wecome to test_boot component!\n");
	prints("|*****************************|\n");

	llboot_comp_init_done();

	while (1) ;
}
