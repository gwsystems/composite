#pragma once

#include "cos_error.h"
#include <chal_types.h>
#include <types.h>
#include <compiler.h>
#include <resources.h>
#include <cos_consts.h>

struct component_ref {
	pgtbl_t                    pgtbl;
	captbl_t                   captbl;
	/*
	 * This is a `struct weakref`, but we have to guarantee that
	 * the pageref_t and the prot_domain_tag_t share the same 64
	 * bit word to make the sinv capability fits into a
	 * cache-line.
	 */
	epoch_t                    epoch;
	pageref_t                  component;
	prot_domain_tag_t          pd_tag;
};

/* Required so that the sinv capability fits into a cache-line */
COS_STATIC_ASSERT(sizeof(struct component_ref) == 4 * sizeof(word_t),
		  "Component reference is larger than expected.");

COS_FASTPATH static inline void
component_ref_copy(struct component_ref *to, struct component_ref *from)
{
	*to = *from;
}

COS_FASTPATH static inline cos_retval_t
component_ref_deref(struct component_ref *comp, pageref_t *ref)
{
	struct page_type *t;

	ref2page(comp->component, NULL, &t);
	if (unlikely(t->epoch != comp->epoch)) return -COS_ERR_NOT_LIVE;
	*ref = comp->component;

	return COS_RET_SUCCESS;
}

struct component {
	pgtbl_ref_t          pgtbl;
	captbl_ref_t         captbl;
	prot_domain_tag_t    pd_tag;
	vaddr_t              entry_ip;

	struct thread       *fault_handler[COS_NUM_CPU];
};
