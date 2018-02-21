#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <hypercall.h>

void cos_init(void)
{
	int r1 = 0, r2 = 0;

	printc("Welcome to the ping component\n");

	printc("Invoking pong interface:\n");
	call();
	call_two();
	call_three();
	call_four();

	printc("\nInvoking pong interface w/ arguments:\n");
	call_arg(1);
	call_args(1, 2, 3, 4);

	printc("\nInvoking pong interface w/ multiple-rets:\n");
	call_3rets(1, 2, 3, 4, &r1, &r2);
	printc(" ping=> r1: %d, r2: %d\n\n", r1, r2);

	hypercall_comp_init_done();

	while (1) ;
}
