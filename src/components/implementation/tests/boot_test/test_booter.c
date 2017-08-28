#include <cos_kernel_api.h>
#include <llprint.h>

#include "boot_deps.h"

void
cos_init(void)
{
	prints("\n|*****************************|\n");
	prints(" Wecome to test_boot component!\n");
	prints("|*****************************|\n");

	cos_sinv(BOOT_SINV_CAP, 1, 2, 3, 4);
}
