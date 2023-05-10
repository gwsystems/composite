#pragma once

#include <chal_types.h>
#include <types.h>
#include <compiler.h>
#include <resources.h>
#include <cos_consts.h>

struct component_ref {
	pgtbl_t                    pgtbl;
	captbl_t                   captbl;
	prot_domain_tag_t          pd_tag;
	struct weak_ref            compref;
};

struct component {
	pgtbl_ref_t          pgtbl;
	captbl_ref_t         captbl;
	prot_domain_tag_t    pd_tag;
	vaddr_t              entry_ip;

	struct thread       *fault_handler[COS_NUM_CPU];
};

COS_FASTPATH static inline int
component_is_alive(struct component_ref *comp)
{
	struct page_type *t;

	ref2page(comp->compref.ref, NULL, &t);

	return t->epoch == comp->compref.epoch;
}
