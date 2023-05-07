#pragma once

#include <cos_arch_types.h>
#include <cos_kern_types.h>

struct component_ref {
	pgtbl_t                    pgtbl;
	captbl_t                   captbl;
	epoch_t                    epoch;
	prot_domain_tag_t          pd_tag;
	pageref_t                  compref;
};

struct component {
	pgtbl_ref_t          pgtbl;
	captbl_ref_t         captbl;
	prot_domain_tag_t    pd_tag;
	vaddr_t              entry_ip;

	struct thread       *fault_handler[COS_NUM_CPU];
};
