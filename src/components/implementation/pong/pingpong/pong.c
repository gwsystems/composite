#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <hypercall.h>

void
call(void)
{
	PRINTLOG(PRINT_DEBUG, "In call() in pong interface, client:%lu\n", cos_inv_token());
	return;
}

void
call_two(void)
{
	PRINTLOG(PRINT_DEBUG, "In call_two() in pong interface, client:%lu\n", cos_inv_token());
	return;
}

void
call_three(void)
{
	PRINTLOG(PRINT_DEBUG, "In call_three() in pong interface, client:%lu\n", cos_inv_token());
	return;
}

void
call_four(void)
{
	PRINTLOG(PRINT_DEBUG, "In call_four() in pong interface, client:%lu\n", cos_inv_token());
	return;
}

void
call_arg(int p1)
{
	PRINTLOG(PRINT_DEBUG, "In call_arg() in pong interface, client:%lu. arg: %d\n", cos_inv_token(), p1);
	return;
}

void
call_args(int p1, int p2, int p3, int p4)
{
	PRINTLOG(PRINT_DEBUG, "In call_args() in pong interface, client:%lu. args: p1:%d p2:%d p3:%d p4:%d\n", cos_inv_token(), p1, p2, p3, p4);
	return;
}

void
call_3rets(int *r2, int *r3, int p1, int p2, int p3)
{
	PRINTLOG(PRINT_DEBUG, "In call_3rets() in pong interface, client:%lu. args: p1:%d p2:%d p3:%d\n", cos_inv_token(), p1, p2, p3);
	*r2 = p1 + p2 + p3;
	*r3 = p1 - p2 - p3;
	return;
}

void
cos_init(void)
{
	int ret;

	PRINTLOG(PRINT_DEBUG, "Welcome to the pong component\n");

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
