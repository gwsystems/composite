#pragma once

#include <types.h>
#include <cos_error.h>
#include <capabilities.h>
#include <chal_consts.h>

struct captbl_internal {
	captbl_t next[COS_CAPTBL_INTERNAL_NENT];
};

struct captbl_leaf {
	struct capability_generic capabilities[COS_PAGE_SIZE / sizeof(struct capability_generic)];
};

cos_retval_t captbl_construct(captbl_ref_t top, captbl_ref_t leaf, uword_t offset);
cos_retval_t captbl_deconstruct(captbl_ref_t top, captbl_ref_t leaf, uword_t offset);

/**
 * `captbl_lookup` finds the capability in a captbl (`ct`) at
 * capability id `cap`. This function *cannot fail*, and the `cap` is
 * treated as wrapping in the capability namespace.
 */
COS_FORCE_INLINE static inline struct capability_generic *
captbl_lookup(captbl_t ct, cos_cap_t cap)
{
	/*
	 * NOTE: we're avoiding a bounds check on the `cap` here, and
	 * are instead wrapping around the captbl if `cap` is larger
	 * than the namespace. This is on the fastpath, so we want to
	 * avoid conditionals.
	 */
	int top_off  = (cap / (COS_CAPTBL_LEAF_NENT)) & (COS_CAPTBL_INTERNAL_NENT - 1);
	int leaf_off = cap & (COS_CAPTBL_LEAF_NENT - 1);

	struct captbl_internal *top  = (struct captbl_internal *)ct;
	struct captbl_leaf     *leaf = (struct captbl_leaf *)top->next[top_off];

	return &leaf->capabilities[leaf_off];
}
