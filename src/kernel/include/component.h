#pragma once

#include <cos_error.h>
#include <chal_types.h>
#include <types.h>
#include <compiler.h>
#include <resources.h>
#include <cos_consts.h>

struct component {
	pgtbl_ref_t          pgtbl;
	captbl_ref_t         captbl;
	prot_domain_tag_t    pd_tag;
	vaddr_t              entry_ip;

	struct thread       *fault_handler[COS_NUM_CPU];
};
