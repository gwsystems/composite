#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <hypercall.h>

void cos_init(void)
{
	int r1 = 0, r2 = 0;
	int a = 1, b = 2, c = 3, d = 4;

	PRINTLOG(PRINT_DEBUG, "Welcome to the ping component\n");

	PRINTLOG(PRINT_DEBUG, "Invoking pong interface:\n");
	call();
	call_two();
	call_three();
	call_four();

	PRINTLOG(PRINT_DEBUG, "Invoking pong interface w/ arguments:\n");
	call_arg(a);
	call_args(a, b, c, d);

	PRINTLOG(PRINT_DEBUG, "Invoking pong interface w/ multiple-rets:\n");
	call_3rets(&r1, &r2, a, b, c);
	PRINTLOG(PRINT_DEBUG, "Returns=> r1: %d, r2: %d\n", r1, r2);
	assert(r1 == (a + b + c));
	assert(r2 == (a - b - c));

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
