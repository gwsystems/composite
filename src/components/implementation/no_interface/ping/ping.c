#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <llboot.h>

void cos_init(void)
{
	printc("Welcome to the ping component\n");

	printc("Invoking pong interface:\n");
	call();
	call_two();
	call_three();
	call_four();

	printc("\nInvoking pong interface w/ arguments:\n");
	call_arg(1);
	call_args(1, 2, 3, 4);

	llboot_comp_init_done();

	while (1) ;
}
