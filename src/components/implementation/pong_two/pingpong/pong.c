#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong_two.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <hypercall.h>

void
call_intern(int u1, int u2, int u3, int u4, int *u5, int *u6)
{
	printc("In call() in pong interface\n");
	return;
}

void
call_two_intern(int u1, int u2, int u3, int u4, int *u5, int *u6)
{
	printc("In call_two() in pong interface. \n");
	return;
}

void
call_three_intern(int u1, int u2, int u3, int u4, int *u5, int *u6)
{
	printc("In call_three() in pong interface. \n");
	return;
}

void
call_four_intern(int u1, int u2, int u3, int u4, int *u5, int *u6)
{
	printc("In call_four() in pong interface. \n");
	return;
}

void
call_arg_intern(int p1,int u1, int u2, int u3, int *u4, int *u5)
{
	printc("In call_arg() in pong interface. arg: %d\n", p1);
	return;
}

void
call_args_intern(int p1, int p2, int p3, int p4, int *u1, int *u2)
{
	printc("In call_args() in pong interface.\n p1:%d p2:%d p3:%d p4:%d \n", p1, p2, p3, p4);
	return;
}

void
call_3rets(int p1, int p2, int p3, int p4, int *p5, int *p6)
{
	printc("In call_3rets() in pong interface.\n p1:%d p2:%d p3:%d p4:%d \n", p1, p2, p3, p4);
	*p5 = p1 - p2 - p3 - p4;
	*p6 = p1 + p2 + p3 + p4;
	return;
}

void
cos_init(void)
{
	int ret;

	printc("Welcome to the pong component\n");

	hypercall_comp_init_done();
	while (1) ;
}
