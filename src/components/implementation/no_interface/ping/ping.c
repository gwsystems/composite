#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>

void cos_init(void)
{
	printc("Welcome to the ping component\n");
	call();	
	printc("Returned from call() invocation from ping to pong\n");
	
	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
	return;
}
