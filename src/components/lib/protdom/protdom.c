
#include <cos_types.h>
#include <protdom.h>
#include <protdom.h>
#include <cos_defkernel_api.h>

#if defined(__x86_64__)

/*
 * - The VAS structure will be an array of pointers to `struct
 *   protdom_comp`s, and "reserved" bits. The reserved bits are used to
 *   notate which names to *split* off, and the `protdom_comp` is which
 *   component has been `alloc_in`ed at that name. Each NS maintains a
 *   pointer to its parent (it split off from).
 *
 * - We also need a similar structure like this to track the MPK
 *   namespace and the ASID namespace.
 */

/* for 32 bit:f
 * name sz = 2^22
 * 2^32 / 2^22 = 2^10 = 1024 names
 * 2^10 / 2 to ensure the array fits into a page = 512 names
 * #define PROTDOM_VAS_NAME_SZ (1 << 22)
 * #define PROTDOM_VAS_NUM_NAMES 512
 * #define PROTDOM_MPK_NUM_NAMES 16
 * #define PROTDOM_ASID_NUM_NAMES 1024
 *
 * for 64 bit:
 * name sz = 2^39
 * 2^48 / 2^39 = 2^9 = 512 names
 * 2^9 / 2 to ensure the array fits into a page = 256 names
 * also: more ASIDs available in 64 bit
 */

#define PROTDOM_VAS_NAME_SZ 		(1ULL << 39)
#define PROTDOM_VAS_NUM_NAMES 		256
#define PROTDOM_MPK_NUM_NAMES 		14
#define PROTDOM_MPK_FIRST_COMP		2
#define PROTDOM_ASID_NUM_NAMES 		4096

#define PROTDOM_NS_STATE_RESERVED 	1
#define PROTDOM_NS_STATE_ALLOCATED 	1 << 1
#define PROTDOM_NS_STATE_ALIASED 	1 << 2


unsigned long
protdom_pgtbl_flags_readable(prot_domain_t protdom)
{
	return COS_PAGE_READABLE | ((unsigned long)protdom.mpk_key << 59);
}

unsigned long
protdom_pgtbl_flags_writable(prot_domain_t protdom)
{
	return COS_PAGE_READABLE | COS_PAGE_WRITABLE | ((unsigned long)protdom.mpk_key << 59);
}

prot_domain_t 
protdom_ns_vas_alloc(protdom_ns_vas_t vas, vaddr_t comp_entry_addr)
{
    prot_domain_t protdom;
	int i, name_index;

	name_index = comp_entry_addr / PROTDOM_VAS_NAME_SZ;
	assert(name_index < PROTDOM_VAS_NUM_NAMES);

	if (!vas->names[name_index].state) return prot_domain_zero();

	for (i = PROTDOM_MPK_FIRST_COMP; i < PROTDOM_MPK_NUM_NAMES ; i++) {
		if ((vas->mpk_names[i].state & (PROTDOM_NS_STATE_RESERVED | PROTDOM_NS_STATE_ALLOCATED)) == PROTDOM_NS_STATE_RESERVED) {
			protdom.mpk_key = i;
            break;
		}
	}

	vas->names[name_index].state |= PROTDOM_NS_STATE_ALLOCATED;

    protdom.asid = vas->asid_name;
    vas->mpk_names[protdom.mpk_key].state |= PROTDOM_NS_STATE_ALLOCATED;

	return protdom;
}

pgtblcap_t
protdom_ns_vas_pgtbl(protdom_ns_vas_t vas)
{
	return vas->top_lvl_pgtbl;
}

void 
protdom_ns_vas_set_comp(protdom_ns_vas_t vas, vaddr_t entry_addr, struct cos_defcompinfo *comp_res)
{
	int name_index = entry_addr / PROTDOM_VAS_NAME_SZ;
	assert(name_index < PROTDOM_VAS_NUM_NAMES);

	vas->names[name_index].comp_res = comp_res;
}


/* Create a new asids namespace */
int
protdom_ns_asids_init(protdom_ns_asid_t asids)
{
	int i;

	for (i = 0 ; i < PROTDOM_ASID_NUM_NAMES ; i++) {
		/* set reserved = 1, allocated = 0 */
		asids->names[i].state = PROTDOM_NS_STATE_RESERVED;
	}

	asids->parent = NULL;

	return 0;
}

/*
 * Create a asid namespace from the names "left over" in `existing`,
 * i.e. those that have not been marked allocated.
 *
 * Return values:
 *    0: success
 *   -1: new is unallocated/null or initialization fails
 *   -2: new already has allocations
 */
int
protdom_ns_asids_split(protdom_ns_asid_t new, protdom_ns_asid_t existing)
{
	int i;

	for (i = 0 ; i < PROTDOM_ASID_NUM_NAMES ; i++) {
		if ((new->names[i].state & PROTDOM_NS_STATE_ALLOCATED) == PROTDOM_NS_STATE_ALLOCATED) return -2;
	}

	if (protdom_ns_asids_init(new)) return -1;

	for (i = 0 ; i < PROTDOM_ASID_NUM_NAMES ; i++) {
		/* if a name is allocated in existing, it should not be reserved in new */
		/* by default via init everything else will go to:
		 *	reserved  = 1
		 *	allocated = 0
		 *	aliased   = 0
		 */
		if ((existing->names[i].state & PROTDOM_NS_STATE_ALLOCATED) == PROTDOM_NS_STATE_ALLOCATED) {
			new->names[i].state &= ~PROTDOM_NS_STATE_RESERVED;
		}
		/* if a name is reserved (but not allocated) in existing, it should no longer be reserved in existing
		 * NOTE: this means no further allocations can be made in existing
		 */
		if ((existing->names[i].state & PROTDOM_NS_STATE_RESERVED) == PROTDOM_NS_STATE_RESERVED) {
			existing->names[i].state &= ~PROTDOM_NS_STATE_RESERVED;
		}
	}

	return 0;
}

/*
 * Return the index of the first available ASID name
 * Return -1 if there are none available
 */
static int
protdom_asid_available_name(protdom_ns_asid_t asids)
{
	int i;

	for (i = 0 ; i < PROTDOM_ASID_NUM_NAMES ; i++) {
		if ((asids->names[i].state & (PROTDOM_NS_STATE_RESERVED | PROTDOM_NS_STATE_ALLOCATED)) == PROTDOM_NS_STATE_RESERVED) {
			return i;
		}
	}

	return -1;
}

/* allocate an asid without creating a VAS namespace */
prot_domain_t
protdom_ns_asid_alloc(protdom_ns_asid_t asids)
{
	int asid = 0;
	prot_domain_t pd;

	asid = protdom_asid_available_name(asids);
	asids->names[asid].state |= PROTDOM_NS_STATE_ALLOCATED;

	pd.asid = asid;
	pd.mpk_key = 0;

	return pd;
}

/*
 * Initialize a new vas namespace, pulling a name from the `asids`
 * Return values:
 *   0: success
 *  -1: new/asids not set up correctly, or no available ASID names, or pgtbl node allocation failed
 */
int
protdom_ns_vas_init(protdom_ns_vas_t new, protdom_ns_asid_t asids)
{
	int asid_index = 0;
	int i = 0;
	pgtblcap_t top_lvl_pgtbl;
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());

	/* find an asid name for new */
	asid_index = protdom_asid_available_name(asids);
	if (asid_index == -1) return -1;

	if ((top_lvl_pgtbl = cos_pgtbl_alloc(ci)) == 0) return -1;

	new->asid_name = asid_index;
	asids->names[asid_index].state |= PROTDOM_NS_STATE_ALLOCATED;

	new->top_lvl_pgtbl = top_lvl_pgtbl;
	new->parent = NULL;

	/* initialize the names in new */
	for (i = 0 ; i < PROTDOM_VAS_NUM_NAMES ; i++) {
		new->names[i].state = PROTDOM_NS_STATE_RESERVED;
		new->names[i].comp_res = NULL;
	}

	/* initialize an MPK NS for new */
	for (i = 0 ; i < PROTDOM_VAS_NUM_NAMES ; i++) {
		new->mpk_names[i].state = PROTDOM_NS_STATE_RESERVED;
	}

	return 0;

}

/*
 * Create a new vas namespace from the names "left over" in
 * `existing`, i.e. those that have not been allocated
 * and automatically alias all names from existing into new
 *
 * Return values:
 *   0: success
 *  -1: new is null/not allocated correctly, or initialization fails
 *  -2: new already has allocations
 *
 * NOTE: after this call, no further allocations can be made in existing
 */
int
protdom_ns_vas_split(protdom_ns_vas_t new, protdom_ns_vas_t existing, protdom_ns_asid_t asids)
{
	int i;
	int cons_ret;

	/* verify that `new` has no existing allocations */
	for (i = 0 ; i < PROTDOM_VAS_NUM_NAMES ; i++) {
		if ((new->names[i].state & PROTDOM_NS_STATE_ALLOCATED) == PROTDOM_NS_STATE_ALLOCATED) return -2;
	}

	if (protdom_ns_vas_init(new, asids)) return -1;

	for (i = 0 ; i < PROTDOM_VAS_NUM_NAMES ; i++) {
		/*
		 * If a name is allocated or aliased in existing, the component there should automatically be aliased into new */
		/* by default via init everything else will go to:
		 * 		reserved  = 1
		 *      allocated = 0
		 *      aliased   = 0
		 */
		if (existing->names[i].state & (PROTDOM_NS_STATE_ALLOCATED | PROTDOM_NS_STATE_ALIASED)) {
			new->names[i].state = (new->names[i].state & ~PROTDOM_NS_STATE_RESERVED) | PROTDOM_NS_STATE_ALIASED;
			new->names[i].comp_res = existing->names[i].comp_res;
			cons_ret = cos_cons_into_shared_pgtbl(cos_compinfo_get(new->names[i].comp_res), new->top_lvl_pgtbl);
			if (cons_ret != 0) BUG();
		}
		/*
		 * If a name is reserved (but not allocated) in existing, it should no longer be reserved in existing
		 * NOTE: this means no further allocations can be made in existing
		 */
		if ((existing->names[i].state & (PROTDOM_NS_STATE_RESERVED | PROTDOM_NS_STATE_ALLOCATED)) == PROTDOM_NS_STATE_RESERVED) {
			existing->names[i].state &= ~PROTDOM_NS_STATE_RESERVED;
		}
	}

	/* initialize the mpk namespace within new */
	for (i = 0 ; i < PROTDOM_MPK_NUM_NAMES ; i++) {
		if ((existing->mpk_names[i].state & PROTDOM_NS_STATE_ALLOCATED) == PROTDOM_NS_STATE_ALLOCATED) {
			new->mpk_names[i].state &= ~PROTDOM_NS_STATE_RESERVED;
		}
		else if ((existing->mpk_names[i].state & (PROTDOM_NS_STATE_RESERVED | PROTDOM_NS_STATE_ALLOCATED)) == PROTDOM_NS_STATE_RESERVED) {
			existing->mpk_names[i].state &= ~PROTDOM_NS_STATE_RESERVED;
		}
	}
	new->parent = existing;

	return 0;
}

int
protdom_ns_vas_shared(protdom_ns_vas_t client, protdom_ns_vas_t server)
{
	if (!client || !server) return 0;

	struct protdom_ns_vas *vas = client;

	while (vas) {
		if (vas == server) return 1;
		vas = vas->parent;
	}

	return 0;
}


/* TODO: provide other implementations */
/* #elseif ... */

/* default implementations */
#else

struct protdom_ns_vas {
	u8_t empty;
};


struct protdom_ns_asid {
	u8_t empty;
};

const size_t PROTDOM_NS_VAS_SIZE  = 0;
const size_t PROTDOM_NS_ASID_SIZE = 0;

int protdom_ns_asids_init(protdom_ns_asid_t asids) {
	return -1;
}

int protdom_ns_asids_split(protdom_ns_asid_t new, protdom_ns_asid_t existing) {
	return -1;
}

int protdom_ns_vas_init(protdom_ns_vas_t new, protdom_ns_asid_t asids) {
	return -1;
}

int protdom_ns_vas_split(protdom_ns_vas_t new, protdom_ns_vas_t existing, protdom_ns_asid_t asids) {
	return -1;
}

int protdom_ns_vas_shared(struct protdom_comp *c1, struct protdom_comp *c2) {
	return -1;
}


unsigned long protdom_pgtbl_flags_readable(prot_domain_t protdom)  {
	return -1;
}

unsigned long protdom_pgtbl_flags_writable(prot_domain_t protdom) {
	return -1;
}

prot_domain_t protdom_alloc(protdom_ns_vas_t vas)  {
	return prot_domain_zero();
}


#endif
