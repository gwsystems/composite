#pragma once

#include <types.h>
#include <chal_consts.h>

struct pgtbl_top {
	pgtbl_t next[COS_PGTBL_TOP_NENT];
	pgtbl_t kern_next[COS_PGTBL_KERN_NENT];
};

struct pgtbl_internal {
	pgtbl_t next[COS_PGTBL_INTERNAL_NENT];
};
