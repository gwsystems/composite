#pragma once

#include <types.h>
#include <consts.h>

struct component {
	pgtbl_ref_t          pgtbl;
	captbl_ref_t         captbl;
	prot_domain_tag_t    pd_tag;
	vaddr_t              entry_ip;

	struct thread       *fault_handler[COS_NUM_CPU];
};
