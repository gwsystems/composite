/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 */

#include "cl_types.h"
#include <stdlib.h>

static void
free_symbs(struct symb_type *st)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		free(st->symbs[i].name);
	}
}

void
free_service_symbs(struct service_symbs *str)
{
	free(str->obj);
	free_symbs(&str->exported);
	free_symbs(&str->undef);
	free(str);

	return;
}
