/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "cl_types.h"
#include "cl_macros.h"
#include "cl_globals.h"
#include "cl_inline.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

inline void
add_kexport(struct service_symbs *ss, const char *name)
{
	struct symb_type *ex = &ss->exported;
	__add_symb(name, ex, 0);
	return;
}

/* FIXME
 * for some reason, using inline functions doesn't mean the function symbol will be generated
 * This means that the add_kexport functions below fail to compile due to missing symbols
 * Adding this declaration forces symbol to be generated
 */
void add_kexport(struct service_symbs *ss, const char *name);

/*
 * Assume that these are added LAST.  The last NUM_KERN_SYMBS are
 * ignored for most purposes so they must be the actual kern_syms.
 *
 * The kernel needs to know where a few symbols are, add them:
 * user_caps, cos_sched_notifications
 */
void
add_kernel_exports(struct service_symbs *service)
{
    add_kexport(service, COMP_INFO);
    add_kexport(service, COMP_PLT);
    add_kexport(service, SCHED_NOTIF);
	return;
}


/* FIXME: should modify code to use
struct service_symbs *get_dependency_by_index(struct service_symbs *s,
                                                     int index)
{
        if (index >= s->num_dependencies) {
                return NULL;
        }

        return s->dependencies[index].dep;
}
*/
int
obs_serialize(asymbol *symb, void *data)
{
	struct symb_type *symbs = data;
	char *            name;

	/* So that we can later add into the exported symbols the user
	 * capability table
	 */
	if (symbs->num_symbs >= MAX_SYMBOLS - NUM_KERN_SYMBS) {
		printl(PRINT_DEBUG,
		       "Have exceeded the number of allowed "
		       "symbols for object %s.\n",
		       symbs->parent->obj);
		return -1;
	}

	/* Ignore main */
	if (!strcmp("main", symb->name)) {
		return 0;
	}

	name = malloc(strlen(symb->name) + 1);
	strcpy(name, symb->name);

	symbs->symbs[symbs->num_symbs].name = name;
	symbs->symbs[symbs->num_symbs].addr = 0;
	symbs->num_symbs++;

	return 0;
}


int
for_each_symb_type(bfd *obj, int symb_type, observer_t o, void *obs_data)
{
	long      storage_needed;
	asymbol **symbol_table;
	long      number_of_symbols;
	int       i;

	storage_needed = bfd_get_symtab_upper_bound(obj);

	if (storage_needed <= 0) {
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}

	symbol_table      = (asymbol **)malloc(storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	for (i = 0; i < number_of_symbols; i++) {
		/*
		 * Invoke the observer if we are interested in a type,
		 * and the symbol is of that type where type is either
		 * undefined or exported, currently
		 */
		if ((symb_type & UNDEF_SYMB_TYPE && bfd_is_und_section(symbol_table[i]->section))
		    || (symb_type & EXPORTED_SYMB_TYPE && symbol_table[i]->flags & BSF_FUNCTION
		        && ((symbol_table[i]->flags & BSF_GLOBAL) || (symbol_table[i]->flags & BSF_WEAK)))) {
			if ((*o)(symbol_table[i], obs_data)) {
				return -1;
			}
		}
	}

	free(symbol_table);

	return 0;
}


/*
 * Fill in the symbols of service_symbs for the object passed in as
 * the tmp_exec
 */
int
obj_serialize_symbols(char *tmp_exec, int symb_type, struct service_symbs *str)
{
	bfd *             obj;
	struct symb_type *st;

	obj = bfd_openr(tmp_exec, "elf32-i386");
	if (!obj) {
		printl(PRINT_HIGH, "Attempting to open %s\n", str->obj);
		bfd_perror("Object open failure");
		return -1;
	}

	if (!bfd_check_format(obj, bfd_object)) {
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}

	if (symb_type == UNDEF_SYMB_TYPE) {
		st = &str->undef;
	} else if (symb_type == EXPORTED_SYMB_TYPE) {
		st = &str->exported;
	} else {
		printl(PRINT_HIGH, "attempt to view unknown symbol type\n");
		exit(-1);
	}
	for_each_symb_type(obj, symb_type, obs_serialize, st);

	bfd_close(obj);

	return 0;
}


int
initialize_service_symbs(struct service_symbs *str)
{
	memset(str, 0, sizeof(struct service_symbs));
	str->exported.parent    = str;
	str->undef.parent       = str;
	str->next               = NULL;
	str->exported.num_symbs = str->undef.num_symbs = 0;
	str->num_dependencies                          = 0;
	str->depth                                     = -1;

	return 0;
}


void
parse_component_traits(char *name, struct component_traits *t, int *off)
{
	switch (name[*off]) {
	case '*': {
		t->sched = 1;
		if (!ROOT_SCHED) {
			char *r = malloc(strlen(name + 1) + 1);
			strcpy(r, name + 1);
			ROOT_SCHED = r;
		}
		break;
	}
	case '!':
		t->composite_loaded = 1;
		break;
	default: /* base case */
		return;
	}
	(*off)++;
	parse_component_traits(name, t, off);

	return;
}


struct service_symbs *
alloc_service_symbs(char *obj)
{
	struct service_symbs *  str;
	char *                  obj_name = malloc(strlen(obj) + 1), *cpy, *orig, *pos;
	const char              lassign = '(', *rassign = ")", *assign = "=";
	struct component_traits t   = {.sched = 0, .composite_loaded = 0};
	int                     off = 0;

	parse_component_traits(obj, &t, &off);
	assert(obj_name);
	/* Do we have a value assignment (a component copy)?  Syntax
	 * is (newval=oldval),... */
	if (obj[off] == lassign) {
		char copy_cmd[256];
		int  ret;

		off++;
		parse_component_traits(obj, &t, &off);

		cpy  = strtok_r(obj + off, assign, &pos);
		orig = strtok_r(pos, rassign, &pos);
		sprintf(copy_cmd, "cp %s %s", orig, cpy);
		ret = system(copy_cmd);
		assert(-1 != ret);
		obj = cpy;
		off = 0;
	}
	printl(PRINT_DEBUG, "Processed object %s (%s%s)\n", obj, t.sched ? "scheduler " : "",
	       t.composite_loaded ? "booted" : "");
	str = malloc(sizeof(struct service_symbs));
	if (!str || initialize_service_symbs(str)) {
		return NULL;
	}

	strcpy(obj_name, &obj[off]);
	str->obj = obj_name;

	str->is_scheduler        = t.sched;
	str->scheduler           = NULL;
	str->is_composite_loaded = t.composite_loaded;
	str->already_loaded      = 0;

	return str;
}


/*
 * Obtain the list of undefined and exported symbols for a collection
 * of services.
 *
 * services is an array of comma delimited addresses to the services
 * we wish to get the symbol information for.  Note that all ',' in
 * the services string are replaced with '\0', and that this function
 * is not thread-safe due to use of strtok.
 *
 * Returns a linked list struct service_symbs data structure with the
 * arrays within each service_symbs filled in to reflect the symbols
 * within that service.
 */
struct service_symbs *
prepare_service_symbs(char *services)
{
	struct service_symbs *str, *first;
	const char *          init_delim = ",", *serv_delim = ";";
	char *                tok, *init_str;
	int                   len;

	printl(PRINT_DEBUG, "Prepare the list of components.\n");

	tok   = strtok(services, init_delim);
	first = str   = alloc_service_symbs(tok);
	init_str      = strtok(NULL, serv_delim);
	len           = strlen(init_str) + 1;
	str->init_str = malloc(len);
	assert(str->init_str);
	memcpy(str->init_str, init_str, len);

	do {
		if (obj_serialize_symbols(str->obj, EXPORTED_SYMB_TYPE, str)
		    || obj_serialize_symbols(str->obj, UNDEF_SYMB_TYPE, str)) {
			printl(PRINT_DEBUG, "Could not open/operate on object %s: error.\n", tok);
			return NULL;
		}
		add_kernel_exports(str);
		tok = strtok(NULL, init_delim);
		if (tok) {
			str->next = alloc_service_symbs(tok);
			str       = str->next;

			init_str      = strtok(NULL, serv_delim);
			len           = strlen(init_str) + 1;
			str->init_str = malloc(len);
			assert(str->init_str);
			memcpy(str->init_str, init_str, len);
		}
	} while (tok);

	return first;
}
