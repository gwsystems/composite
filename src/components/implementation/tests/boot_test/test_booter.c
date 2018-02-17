#include <cos_kernel_api.h>
#include <llprint.h>
#include <llbooter_inv.h>

#include "boot_deps.h"

void
cos_init(void)
{
	prints("\n|*****************************|\n");
	prints(" Wecome to test_boot component!\n");
	prints("|*****************************|\n");

	cos_hypervisor_hypercall(INIT_DONE, 0, 0, 0);
}
