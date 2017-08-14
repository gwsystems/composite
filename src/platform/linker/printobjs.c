/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 */

#include "cl_types.h"
#include "cl_macros.h"
#include "cl_globals.h"

#include <cos_config.h>
#include <cobj_format.h>

#include <stdio.h>

static inline void
print_symbs(struct symb_type *st)
{
	int i;

	for (i = 0; i < st->num_symbs; i++) {
		printl(PRINT_DEBUG, "%s, ", st->symbs[i].name);
	}

	return;
}

void
print_objs_symbs(struct service_symbs *str)
{
	if (print_lvl < PRINT_DEBUG) return;
	while (str) {
		printl(PRINT_DEBUG, "Service %s:\n\tExported functions: ", str->obj);
		print_symbs(&str->exported);
		printl(PRINT_DEBUG, "\n\tUndefined: ");
		print_symbs(&str->undef);
		printl(PRINT_DEBUG, "\n\n");

		str = str->next;
	}

	return;
}
