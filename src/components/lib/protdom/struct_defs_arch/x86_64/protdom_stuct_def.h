
#include <cos_types.h>
#include <cos_kernel_api.h>

#define PROTDOM_VAS_NAME_SZ 	(1ULL << 39)
#define PROTDOM_VAS_NUM_NAMES 	256
#define PROTDOM_MPK_NUM_NAMES 	14
#define PROTDOM_ASID_NUM_NAMES 	4096

#define PROTDOM_NS_STATE_RESERVED 	1
#define PROTDOM_NS_STATE_ALLOCATED 1 << 1
#define PROTDOM_NS_STATE_ALIASED 	1 << 2


struct protdom_vas_name {
	u32_t state : 3;
	struct cos_defcompinfo *comp_res;
};

struct protdom_asid_mpk_name {
	u32_t state : 3;
};

/* we need to expose these definitions for the static slab allocators in the booter */

struct protdom_ns_vas {
	pgtblcap_t top_lvl_pgtbl;
	struct protdom_vas_name names[PROTDOM_VAS_NUM_NAMES];
	struct protdom_ns_vas *parent;
	u32_t asid_name;
	struct protdom_asid_mpk_name mpk_names[PROTDOM_MPK_NUM_NAMES];
};

struct protdom_ns_asid {
	struct protdom_asid_mpk_name names[PROTDOM_ASID_NUM_NAMES];
	struct protdom_ns_asid *parent;
};