#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <hypercall.h>

void cos_init(void)
{
	int r = 0, r1 = 0, r2 = 0;
	int a = 10, b = 20, c = 30, d = 40;

	PRINTLOG(PRINT_DEBUG, "Welcome to the ping component\n");

	PRINTLOG(PRINT_DEBUG, "Invoking pong interface:\n");
	r = call();
	assert(r == 0);
	r = call_two();
	assert(r == 2);
	r = call_three();
	assert(r == 3);
	r = call_four();
	assert(r == 4);

	PRINTLOG(PRINT_DEBUG, "Invoking pong interface w/ arguments:\n");
	r = call_arg(a);
	assert(r == a);
	r = call_args(a, b, c, d);
	assert(r == a);

	PRINTLOG(PRINT_DEBUG, "Invoking pong interface w/ multiple-rets:\n");
	r = call_3rets(&r1, &r2, a, b, c, d);
	assert(r == a);
	PRINTLOG(PRINT_DEBUG, "Returns=> r1: %d, r2: %d\n", r1, r2);
	assert(r1 == (a + b + c + d));
	assert(r2 == (a - b - c - d));

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
