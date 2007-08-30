/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#ifndef SPD_H
#define SPD_H

#ifndef ASM

#include <cos_types.h>
#include <consts.h>

/**
 * Service Protection Domains
 *
 * Protection domains for different services or components do not
 * consist of entire address spaces defined by hardware page tables.
 * Instead, a single address space is divided between all of the
 * services within an application's execution domain.  In a sense, an
 * application and all its services exist in a private single address
 * space.  However, this is not a single address space OS as ALL
 * services across the entire system for all applications exist in the
 * single address space.  
 *
 * This file defines the structures and values for defining service
 * protection domains.  
 */

#define MAX_NUM_SPDS 20
/* ~ ((pagesize - sizeof(spd))/sizeof(static_capability)) */
#define MAX_STATIC_CAP 1000

/*
 * Static capabilities:
 * 
 * The user-level version of the static capability. A vector of these
 * is mapped into each spd user-level address space.  It contains the
 * address of the function to invoke to make the ipc, and the address
 * of the function in the trusted service to call, a count of the
 * number of times invocations have been made with this capability,
 * and the capability number for this entry.
 */
struct usr_inv_cap {
	vaddr_t invocation_fn, service_entry_inst;
	unsigned int invocation_count, cap_no;
} HALF_CACHE_ALIGNED;

/* ST stubs are linked automatically */
struct usr_cap_stubs {
	vaddr_t SD_cli_stub, SD_serv_stub;
	vaddr_t AT_cli_stub, AT_serv_stub;
};

/* 
 * Structure defining the information contained in a static capability
 * for invocation with trust.
 */
struct spd;
struct invocation_cap {
	/* the spd that can make invocations on this capability
	 * (owner), and the spd that invocations are made to. owner ==
	 * NULL means that this entry is free and not in use.  */
	struct spd *owner, *destination;
	unsigned int invocation_cnt:30;
	isolation_level_t il:2;
	vaddr_t dest_entry_instruction;
	struct usr_cap_stubs usr_stub_info;
} CACHE_ALIGNED;


/* end static capabilities */


/*
 * The Service Protection Domain description including 
 *
 * - location information inside an address space, and a page table.
 * - size information regarding the static capabilities (max and
 *   current size)
 * - the identification permissions (which identification bytes this
 *   spd can control.)
 * - The address of the user-level static capabilities structure
 * - The static capabilities for this service.
 */

/* spd flags */
#define SPD_COMPOSITE  0x1 // Is this spd a composite_spd?
#define SPD_FREE       0x2 // currently unused
#define SPD_DEPRICATED 0x4 // Must have SPD_COMPOSITE.  This spd
                           // should no longer be used except for
			   // thread returns.

/*
 * The spd_poly struct contains all the information that both struct
 * spds and struct composite_spds contains.  In essence, we want them
 * to be polymorphic with spd_poly as the "base class".
 */
struct spd_poly {
	unsigned int flags;
	phys_addr_t pg_tbl;
	atomic_t ref_cnt;
};

/* 
 * A collection of Symmetrically trusted spds.
 *
 * Note the though this structure has a reference count which counts
 * the number of threads that have invoked this service but not
 * returned, the reference count is really the sum of the reference
 * counts for all spds in this composite.
 *
 * This is done to reduce the number of cache/tlb misses by one (no
 * composite_spd access on IPC fast path).
 */
struct composite_spd {
	struct spd_poly spd_info;
	unsigned int num_element_spds;
	struct spd *spds[];
} CACHE_ALIGNED;

/* Currently spds consist of a contiguous range of virtual addresses. */
struct spd_location {
	unsigned int lowest_addr;
	unsigned int size;
};

struct spd {
	/* data touched on the ipc hotpath (32 bytes)*/
	struct spd_poly spd_info;
	struct spd_location location;
	struct composite_spd *composite_spd;
	
	unsigned short int cap_base, cap_range, cap_used;
	struct usr_inv_cap *user_cap_tbl;

	phys_addr_t local_pg_tbl;
	atomic_t local_ref_cnt;
	/*
	 * id_cap
	 * 
	 */

	/* should be a union to not waste space */
	struct spd *freelist_next;
} CACHE_ALIGNED; //cache line size

struct spd *spd_alloc(unsigned short int max_static_cap, struct usr_inv_cap *usr_cap_tbl);
void spd_free(struct spd *spd);
void spd_init(void);

unsigned int spd_add_static_cap(struct spd *spd, vaddr_t service_entry_inst, struct spd *trusted_spd, 
				isolation_level_t isolation_level);

#else /* ASM */

/* WRONG */
#define SPD_CAP_TBL_PTR 8
#define SPD_CAP_TBL_SZ 12
#define SPD_LOWER_ADDR 0
#define SPD_REGION_SIZE 4


#define CAP_SIZE 8
#define CAP_ENTRY_INST 0
#define CAP_SPD 4

#endif

#endif /* SPD_H */
