#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong.h>
#include <cos_debug.h>
#include <cos_types.h>

enum {
	BOOT_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};

void 
call(void) { 
	printc("LETS GET IT\n");
	return; 
}

void
cos_init(void)
{
	printc("Welcome to the pong component\n");

	cos_sinv(BOOT_SINV_CAP, 1, 2, 3, 4);
}
