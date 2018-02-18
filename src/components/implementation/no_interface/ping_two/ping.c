#include <cos_kernel_api.h>
#include <pong_two.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <llboot.h>

void cos_init(void)
{
	int r1 = 0, r2 = 0;

	printc("Welcome to the ping_two component\n");

	printc("Invoking pong_two interface:\n");
	call();
	call_two();
	call_three();
	call_four();

	printc("\nInvoking pong_two interface w/ arguments:\n");
	call_arg(1);
	call_args(1, 2, 3, 4);

	printc("\nInvoking pong_two interface w/ multiple-rets:\n");
	call_3rets(1, 2, 3, 4, &r1, &r2);
	printc(" ping_two=> r1: %d, r2: %d\n\n", r1, r2);

	llboot_comp_init_done();

	while (1) ;
}
