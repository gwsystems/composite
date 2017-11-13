#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong_two.h>
#include <cos_debug.h>
#include <cos_types.h>

void 
call(void) { 
	printc("In call() in pong_two interface\n");
	return; 
}

void 
call_two(void) { 
	printc("In call_two() in pong_two interface. \n");
	return; 
}

void 
call_three(void) { 
	printc("In call_three() in pong_two interface. \n");
	return; 
}

void 
call_four(void) { 
	printc("In call_four() in pong_two interface. \n");
	return; 
}

void 
call_arg(int p1) { 
	printc("In call_arg() in pong_two interface. arg: %d\n", p1);
	return; 
}

void 
call_args(int p1, int p2, int p3, int p4) { 
	printc("In call_args() in pong_two interface.\n p1:%d p2:%d p3:%d p4:%d \n", p1, p2, p3, p4);
	return; 
}

void
cos_init(void)
{
	printc("Welcome to the pong _twocomponent\n");

	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
}
