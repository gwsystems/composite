#include <cos_kernel_api.h>
#include <cos_component.h>
#include <llprint.h>
#include <llbooter_inv.h>

#include "boot_deps.h"

void
cos_init(void)
{
	struct cos_config_info_t *my_info;

	prints("\n|*****************************|\n");
	prints(" Wecome to test_boot component!\n");
	prints("|*****************************|\n");

	printc("Fetching boot configuration information\n");
	my_info = cos_init_args();
	printc("Greeting key: %s\n", my_info->kvp[GREETING_KEY].key);
	printc("Greeting value: %s\n", my_info->kvp[GREETING_KEY].value);

	cos_hypervisor_hypercall(BOOT_HYP_INIT_DONE, 0, 0, 0);
}
