#pragma once

#include <types.h>
#include <cos_types.h>
#include <cos_error.h>

cos_retval_t kernel_init(uword_t post_constructor_offset);
void         kernel_core_init(coreid_t coreid);
cos_retval_t constructor_init(uword_t post_constructor_offset, vaddr_t constructor_lower_vaddr, vaddr_t constructor_entry,
			      uword_t ro_off, uword_t ro_sz, uword_t data_off, uword_t data_sz, uword_t zero_sz);
COS_NO_RETURN void constructor_core_execute(coreid_t coreid, vaddr_t entry_ip);
