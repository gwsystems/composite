#ifndef PROTDOM_H
#define PROTDOM_H

#include <cos_types.h>


/* we need to expose these definitions for the static slab allocators in the booter */
#if defined(__x86_64__)
	#include "struct_defs_arch/x86_64/protdom_stuct_def.h"
#else
	#include "struct_defs_arch/default/protdom_stuct_def.h"
#endif

/* structures are architecure defined, leave them opaque to the interface */
typedef struct protdom_ns_vas  *protdom_ns_vas_t;
typedef struct protdom_ns_asid *protdom_ns_asid_t;

/**
 * NS creation:
 *
 * - create functions that simply create a new ns with all of the
 *   names available, and
 * - split functions that simply make a new ns from the unallocated
 *   names left over in an existing namespace.
 */

/* Create a new asids namespace */
int protdom_ns_asids_init(protdom_ns_asid_t asids);

/*
 * Create a asid namespace from the names "left over" in `existing`,
 * i.e. those that have not been `protdom_ns_vas_alloc_in`ed.
 */
int protdom_ns_asids_split(protdom_ns_asid_t new, protdom_ns_asid_t existing);

/* Initialize a new vas namespace, pulling a name from the `asids`*/
int protdom_ns_vas_init(protdom_ns_vas_t new, protdom_ns_asid_t asids);

/*
 * Create a new vas namespace from the names "left over" in
 * `existing`, i.e. those that have not been `protdom_ns_vas_alloc_in`ed
 */
int protdom_ns_vas_split(protdom_ns_vas_t new, protdom_ns_vas_t existing, protdom_ns_asid_t asids);

int protdom_ns_vas_shared(protdom_ns_vas_t c1, protdom_ns_vas_t c2);

pgtblcap_t protdom_ns_vas_pgtbl(protdom_ns_vas_t vas);

void protdom_ns_vas_set_comp(protdom_ns_vas_t vas, vaddr_t entry_addr, struct cos_defcompinfo *comp_res);


unsigned long protdom_pgtbl_flags_readable(prot_domain_t protdom);
unsigned long protdom_pgtbl_flags_writable(prot_domain_t protdom);

prot_domain_t protdom_ns_vas_alloc(protdom_ns_vas_t vas, vaddr_t comp_entry_addr);

/* allocate an asid without creating a VAS namespace */
prot_domain_t protdom_ns_asid_alloc(protdom_ns_asid_t asids);

#endif