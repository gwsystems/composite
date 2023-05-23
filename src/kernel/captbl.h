#pragma once

#include "component.h"
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
/**
 * `captbl_cap_typecheck` takes a capability, checks that it is live,
 * has the expected `type`, and has permissions that allow the
 * `required` operations. Note that `required` can be `0`, in which
 * the permissions are *not* checked. This is the fastpath for `sinv`
 * to avoid any checks (permissions for `sinv` are vacuous -- the
 * capability alone denotes a binary permission).
 */
static inline cos_retval_t
captbl_cap_typecheck(struct capability_generic* c, cos_cap_type_t type, cos_op_bitmap_t required)
{
	if (unlikely(c->type != type)) return -COS_ERR_WRONG_CAP_TYPE;
        /*
         * We first test required being zero to make it clear to the
         * compiler to omit this test in that case (i.e. the sinv
         * fastpath).
	 */
	if (unlikely(required && (c->operations & required) == required)) return -COS_ERR_INSUFFICIENT_PERMISSIONS;

	return COS_RET_SUCCESS;
}

/**
 * `captbl_lookup_type` does a capability lookup (looking up `cap` in
 * `ct`), and checks liveness, `type`, and if the capability provides
 * permissions to allow the `required` operations. Returns a normal
 * error/success, and the capability in `cap_ret`. Note that the
 * pointer might be populated even in the case of an error, in which
 * case you should ignore the value.
 */
cos_retval_t
captbl_lookup_type(captbl_t ct, cos_cap_t cap, cos_cap_type_t type, cos_op_bitmap_t required, struct capability_generic **cap_ret)
{
	*cap_ret = captbl_lookup(ct, cap);

	return captbl_cap_typecheck(*cap_ret, type, required);
}

/* Simple helper to do the cast to the super-type for us */
#define CAPTBL_LOOKUP_TYPE(ct, cap, type, required, cap_ret) \
	captbl_lookup_type(ct, cap, type, required, (struct capability_generic **)&cap_ret)

/* Forced inline here is meant to remove the switch's conditions */
COS_FORCE_INLINE static inline cos_retval_t
captbl_lookup_cap_deref(struct capability_generic *capslot, cos_cap_type_t captype, cos_op_bitmap_t required, pageref_t *ref)
{
	struct weak_ref *wref;
	struct component_ref *compref;

	/* pull the resource reference out of the capability if the reference is valid */
	switch (captype) {
	case COS_CAP_TYPE_COMP:
		return component_ref_deref(&((struct capability_component *)capslot)->intern.component, ref);
	case COS_CAP_TYPE_SINV:
		return component_ref_deref(&((struct capability_sync_inv *)capslot)->intern.component, ref);
	case COS_CAP_TYPE_HW:	/* No page associated with hardware capabilities */
		return -COS_ERR_WRONG_CAP_TYPE;
	default:		/* the rest of the types have a shared capability slot structure */
		return resource_weakref_deref(&(((struct capability_resource *)capslot)->intern.ref), ref);
	}
}

COS_FORCE_INLINE static inline cos_retval_t
captbl_lookup_cap_type_deref(struct capability_generic *capslot, cos_cap_type_t captype, cos_op_bitmap_t required, pageref_t *ref)
{
	COS_CHECK(captbl_cap_typecheck(capslot, captype, required));

	return captbl_lookup_cap_deref(capslot, captype, required, ref);
}

/**
 * `captbl_lookup_type_deref` finds a `pageref_t` associated with a
 * capability *only if* its type matches `captype`, it includes the
 * permissions required in `required`, and the reference to the page
 * isn't out of date.
 */
COS_FORCE_INLINE static inline cos_retval_t
captbl_lookup_type_deref(captbl_t ct, cos_cap_t cap, cos_cap_type_t captype, cos_op_bitmap_t required, pageref_t *ref)
{
	struct capability_generic *capslot;

	COS_CHECK(captbl_lookup_type(ct, cap, captype, required, &capslot));

	return captbl_lookup_cap_deref(capslot, captype, required, ref);
}


cos_retval_t cap_comp_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t comp_ref);
cos_retval_t cap_sinv_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, pageref_t comp_ref, vaddr_t entry_ip, inv_token_t token);
cos_retval_t cap_thd_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t thd_ref);
cos_retval_t cap_restbl_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, page_kerntype_t kt, cos_op_bitmap_t operations, pageref_t restbl_ref);

cos_retval_t capability_remove(pageref_t captblref, uword_t off);
cos_retval_t capability_copy(pageref_t captblref_to, uword_t off_to, pageref_t captblref_from, uword_t off_from,
                             cos_op_bitmap_t ops);
