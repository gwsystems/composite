#pragma once

#include <chal_types.h>
#include <cos_types.h>

/*
 * Kernel value types
 */
typedef uword_t        kaddr_t;           /* kernel virtual address, directly dereferenceable */
typedef u32_t          pageref_t;         /* opaque page reference. Can be used to index into the page_types, and pages */
typedef uword_t        paddr_t;	          /* physical address */
typedef uword_t        captbl_t;          /* reference to a capability table that can be dereferenced */
typedef pageref_t      pgtbl_ref_t;       /* indirect reference to a page-table */
typedef pageref_t      captbl_ref_t;      /* indirect reference to a capability-table */
typedef u64_t          liveness_t;        /* liveness value to determine if there are parallel references */
typedef u64_t          epoch_t;	          /* epoch to be used for versioned pointers */
typedef u64_t          refcnt_t;          /* type of a specific reference counter */

typedef u8_t           page_type_t;       /* tag for each page's type+kerntype used for retyping */
typedef u8_t           page_kerntype_t;

typedef u32_t          cos_cap_type_t;    /* tag for each capability's type */

/*
 * A versioned reference to a resource. Needs to be checked for
 * validity before dereference (i.e. checking that the epoch here
 * matches that of the resource). These weak references are used
 * throughout the capability-tables, this enabling the removal of
 * resources without needing to remove them from the resource table.
 * This is a strong form of atomic revocation that is necessary for
 * components (i.e. to deallocate and/or signal failure), and enables
 * management components to reuse resources easily without bookkeeping
 * as they can elide capability-table management. This also is quite
 * significant for the parallelism properties of the system as it
 * enables us to avoid reference counts for capability-table slot
 * references. This makes it much more safe for any component to be
 * able to manage its own capability tables.
 *
 * The core APIs for managing these references are
 * `resource_weakref_*` in `resources.h`.
 */
struct weak_ref {
	epoch_t   epoch;
	pageref_t ref;
} __attribute__((packed));
