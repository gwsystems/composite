#pragma once

#include <types.h>
#include <cos_types.h>
#include <cos_error.h>

struct kernel_init_state {
	uword_t thread_offset;
	uword_t post_constructor_offset;
};

cos_retval_t kernel_init(uword_t post_constructor_offset, struct kernel_init_state *state);
void         kernel_cores_init(struct kernel_init_state *state);
cos_retval_t constructor_init(vaddr_t constructor_lower_vaddr, vaddr_t constructor_entry, uword_t ro_off, uword_t ro_sz,
                              uword_t data_off, uword_t data_sz, uword_t zero_sz, struct kernel_init_state *state);
COS_NO_RETURN void constructor_core_execute(coreid_t coreid, struct kernel_init_state *s);
