#include <cos_types.h>

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

enum {
       	BOOT_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE  + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};
