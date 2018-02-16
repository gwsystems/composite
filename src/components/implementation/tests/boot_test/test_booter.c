#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>

void
cos_init(void)
{
	prints("\n|*****************************|\n");
	prints(" Wecome to test_boot component!\n");
	prints("|*****************************|\n");

	cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, 0, 0, 0);
}
