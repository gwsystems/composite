#include <cos_debug.h>
#include <cos_types.h>

enum {
	BOOT_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};
