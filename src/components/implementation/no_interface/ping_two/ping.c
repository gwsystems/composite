#include <cos_kernel_api.h>
#include <pong_two.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <llbooter_inv.h>

void cos_init(void)
{
	printc("\nIn the ping_two component\n");

	printc("Ping_two: Invoking pong interface:\n");
	call();
	call_two();
	call_three();
	call_four();

	printc("\nPing_two: Invoking pong interface w/ arguments\n");
	call_arg(1);
	call_args(1, 2, 3, 4);

	cos_hypervisor_hypercall(BOOT_HYP_INIT_DONE, 0, 0, 0);
	return;
}
