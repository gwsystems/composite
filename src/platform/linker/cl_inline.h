#ifndef CL_INLINE_H
#define CL_INLINE_H

#include "cl_types.h"
#include "cl_macros.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static inline int
is_booter_loaded(struct service_symbs *s)
{
	return (!(strstr(s->obj, INIT_COMP) || strstr(s->obj, LLBOOT_COMP)));
	//      return s->is_composite_loaded;
}


static inline vaddr_t
get_symb_address(struct symb_type *st, const char *symb)
{
	int i;

	for (i = 0; i < st->num_symbs; i++) {
		if (!strcmp(st->symbs[i].name, symb)) {
			return st->symbs[i].addr;
		}
	}
	return 0;
}

static inline int
fault_handler_num(char *fn_name)
{
	int i;

	for (i = 0; i < COS_FLT_MAX ; i++) {
		if (!strcmp(cos_flt_handlers[i], fn_name)) return i;
	}
	return -1;
}

static inline void
__add_symb(const char *name, struct symb_type *exp_undef, int mod_len)
{
	exp_undef->symbs[exp_undef->num_symbs].name            = malloc(strlen(name) + 1);
	exp_undef->symbs[exp_undef->num_symbs].modifier_offset = mod_len;
	strcpy(exp_undef->symbs[exp_undef->num_symbs].name, name);
	exp_undef->num_symbs++;
	assert(exp_undef->num_symbs <= MAX_SYMBOLS);
}

static inline void
add_undef_symb(struct service_symbs *ss, const char *name, int mod_len)
{
	struct symb_type *ud = &ss->undef;
	__add_symb(name, ud, mod_len);
	return;
}

static inline struct cos_sections *
cos_sect_get(sec_type_t id)
{
	int        i;
	static int first = 1;

	assert(id <= MAXSEC_S);
	if (first) {
		for (i = 0; section_info[i].secid != MAXSEC_S; i++) {
			assert(section_info[i].secid == (unsigned int)i);
		}
	}
	return &section_info[id];
}

#define csg(id) cos_sect_get(id)


#endif
