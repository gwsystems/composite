#pragma once

#include <cos_types.h>
#include <consts.h>

/*
 * Kernel value types
 */
typedef uword_t        kaddr_t;           /* kernel virtual address, directly dereferenceable */
typedef uword_t        paddr_t;	          /* physical addresss */
typedef uword_t        vaddr_t;           /* opaque, user-level virtual address */
typedef u32_t          pageref_t;         /* opaque page reference. Can be used to index into the page_types, and pages */
typedef paddr_t        pgtbl_t;	          /* reference to a page-table that can be loaded into the CPU */
typedef uword_t        captbl_t;          /* reference to a capability table that can be dereferenced */
typedef pageref_t      pgtbl_ref_t;       /* indirect reference to a page-table */
typedef pageref_t      captbl_ref_t;      /* indirect reference to a capability-table */
typedef u64_t          liveness_t;        /* liveness value to determine if there are parallel references */
typedef u64_t          epoch_t;	          /* epoch to be used for versioned pointers */
typedef u64_t          refcnt_t;          /* type of a specific reference counter */

typedef u8_t           page_type_t;       /* tag for each page's type+kerntype used for retyping */
typedef u8_t           page_kerntype_t;

typedef u16_t          cos_cap_type_t;    /* tag for each capability's type */
