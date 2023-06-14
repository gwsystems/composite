#pragma once

#include <resources.h>
#include <cos_error.h>
#include <chal_pgtbl.h>

cos_retval_t pgtbl_leaf_lookup(pgtbl_ref_t pgtbl_ref, uword_t pgtbl_src_off, page_type_t expected_type,
                               page_kerntype_t expected_kerntype, uword_t required_perm, pageref_t *resource_ref);
int          page_is_pgtbl(page_kerntype_t type);
cos_retval_t pgtbl_unmap(pgtbl_ref_t pt, uword_t offset);
cos_retval_t pgtbl_map(pgtbl_ref_t pt, uword_t offset, pageref_t page, uword_t perm);
cos_retval_t pgtbl_deconstruct(pgtbl_ref_t top, uword_t offset);
cos_retval_t pgtbl_construct(pgtbl_ref_t top, uword_t offset, pgtbl_ref_t bottom, uword_t perm);
cos_retval_t pgtbl_copy(pgtbl_ref_t to, uword_t to_off, pgtbl_ref_t from, uword_t from_off, uword_t perm);
