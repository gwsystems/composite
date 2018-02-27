#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llbooter_inv.h>

void
call(void) {
	printc("In call() in pong interface\n");
	return;
}

void
call_two(void) {
	printc("In call_two() in pong interface. \n");
	return;
}

void
call_three(void) {
	printc("In call_three() in pong interface. \n");
	return;
}

void
call_four(void) {
	printc("In call_four() in pong interface. \n");
	return;
}

void
call_arg(int p1) {
	printc("In call_arg() in pong interface. arg: %d\n", p1);
	return;
}

void
call_args(int p1, int p2, int p3, int p4) {
	printc("In call_args() in pong interface.\n p1:%d p2:%d p3:%d p4:%d \n", p1, p2, p3, p4);
	return;
}

void
cos_init(void)
{
	int ret;

	printc("Welcome to the pong component\n");

	cos_hypervisor_hypercall(BOOT_HYP_INIT_DONE, 0, 0, 0);
}
