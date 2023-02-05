
#include <cos_types.h>
#include <protdom.h>
#include <cos_kernel_api.h>

/* we need to expose these definitions for the static slab allocators in the booter */

struct protdom_ns_vas {
	char empty;
};

struct protdom_ns_asid {
	char empty;
};