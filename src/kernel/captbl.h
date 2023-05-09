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
#define CAPTBL_LOOKUP_TYPE(ct, cap, type, required, cap_ret)                   \
  captbl_lookup_type(ct, cap, type, required,                                  \
                     (struct capability_generic **)&cap_ret)

cos_retval_t cap_create_comp(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off,
                             cos_op_bitmap_t operations, captbl_ref_t captbl_ref, pgtbl_ref_t pgtbl_ref,
                             prot_domain_tag_t pd, vaddr_t entry_ip, pgtbl_ref_t untyped_src_ref);
cos_retval_t cap_create_sinv(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, pageref_t comp_ref,
                             vaddr_t entry_ip, inv_token_t token);
cos_retval_t cap_create_thd(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations,
                            pageref_t sched_ref, pageref_t tcap_ref, pageref_t comp_ref, epoch_t epoch, thdid_t id,
                            id_token_t token, pgtbl_ref_t untyped_src_ref);
cos_retval_t cap_create_restbl(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, page_kerntype_t kt,
                               cos_op_bitmap_t operations, pgtbl_ref_t untyped_src_ref);

int capability_is_captbl(struct capability_generic *c);
int capability_is_pgtbl(struct capability_generic *c);
