#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong.h>
#include <cos_debug.h>
#include <cos_types.h>

void 
call(void) { 
	printc("In call() in pong interface\n");
	return; 
}

void
cos_init(void)
{
	printc("Welcome to the pong component\n");

	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
}
