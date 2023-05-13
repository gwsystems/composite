#pragma once

#include "cos_consts.h"
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
cos_retval_t captbl_deconstruct(captbl_ref_t top, uword_t offset);
void         captbl_leaf_initialize(struct captbl_leaf *ct);
void         captbl_intern_initialize(struct captbl_internal *ct);
int          page_is_captbl(page_kerntype_t type);
cos_retval_t captbl_lookup_type(captbl_t ct, cos_cap_t cap, cos_cap_type_t type, cos_op_bitmap_t required,
                                struct capability_generic **cap_ret);
struct capability_generic *captbl_leaf_lookup(struct captbl_leaf *captbl, uword_t leaf_off);

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

/* Simple helper to do the cast to the super-type for us */
#define CAPTBL_LOOKUP_TYPE(ct, cap, type, required, cap_ret) \
	captbl_lookup_type(ct, cap, type, required, (struct capability_generic **)&cap_ret)

/* Forced inline here is meant to remove the switch's conditions */
COS_FORCE_INLINE static inline cos_retval_t
captbl_lookup_type_deref(captbl_t ct, cos_cap_t cap, cos_cap_type_t captype, cos_op_bitmap_t required, pageref_t *ref)
{
	struct capability_generic *capslot;
	struct weak_ref *wref;

	COS_CHECK(captbl_lookup_type(ct, cap, captype, required, &capslot));

	/* pull the resource reference out of the capability if the reference is valid */
	switch (captype) {
	case COS_CAP_TYPE_COMP:
		wref = &(((struct capability_component *)capslot)->intern.component.compref);
		break;
	case COS_CAP_TYPE_SINV:
		wref = &(((struct capability_sync_inv *)capslot)->intern.component.compref);
		break;
	case COS_CAP_TYPE_HW:	/* No page associated with hardware capabilities */
		return -COS_ERR_WRONG_CAP_TYPE;
	default:		/* the rest of the types have a shared capability slot structure */
		wref = &(((struct capability_resource *)capslot)->intern.ref);
		break;
	}

	COS_CHECK(resource_weakref_deref(wref, ref));

	return COS_RET_SUCCESS;
}

cos_retval_t cap_comp_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t comp_ref);
cos_retval_t cap_sinv_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, pageref_t comp_ref, vaddr_t entry_ip, inv_token_t token);
cos_retval_t cap_thd_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t thd_ref);
cos_retval_t cap_restbl_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, page_kerntype_t kt, cos_op_bitmap_t operations, pageref_t restbl_ref);

int capability_is_captbl(struct capability_generic *c);
int capability_is_pgtbl(struct capability_generic *c);
