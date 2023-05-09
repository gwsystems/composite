#pragma once

#include <resources.h>
#include <cos_error.h>
#include <chal_pgtbl.h>

cos_retval_t pgtbl_leaf_lookup(pgtbl_ref_t pgtbl_ref, uword_t pgtbl_src_off, page_type_t expected_type,
                               page_kerntype_t expected_kerntype, uword_t required_perm, pageref_t *resource_ref);
int          page_is_pgtbl(page_kerntype_t type);
void         pgtbl_top_initialize(struct pgtbl_top *pt);
void         pgtbl_intern_initialize(struct pgtbl_internal *pt);
