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

int
__add_service_dependency(struct service_symbs *s, struct service_symbs *dep, char *modifier, int mod_len)
{
	struct dependency *d;

	if (!s || !dep || s->num_dependencies == MAX_TRUSTED) {
		return -1;
	}
	if (!is_booter_loaded(s) && is_booter_loaded(dep)) {
		printl(PRINT_HIGH, "Error: Non-Composite-loaded component dependent on composite loaded component.\n");
		return -1;
	}

	d           = &s->dependencies[s->num_dependencies];
	d->dep      = dep;
	d->modifier = modifier;
	d->mod_len  = mod_len;
	d->resolved = 0;
	s->num_dependencies++;

	return 0;
}


int
add_service_dependency(struct service_symbs *s, struct service_symbs *dep)
{
	return __add_service_dependency(s, dep, NULL, 0);
}


int
add_modified_service_dependency(struct service_symbs *s, struct service_symbs *dep, char *modifier, int mod_len)
{
	char *new_mod;

	assert(modifier);
	new_mod = malloc(mod_len + 1);
	assert(new_mod);
	memcpy(new_mod, modifier, mod_len);
	new_mod[mod_len] = '\0';

	return __add_service_dependency(s, dep, new_mod, mod_len);
}


inline struct service_symbs *
get_service_struct(char *name, struct service_symbs *list)
{
	while (list) {
		assert(name);
		assert(list && list->obj);
		if (!strcmp(name, list->obj)) {
			return list;
		}

		list = list->next;
	}

	return NULL;
}

struct service_symbs * get_service_struct(char *name, struct service_symbs *list);

/*
 * Add to the service_symbs structures the dependents.
 *
 * deps is formatted as "sa-sb|sc|...|sn;sd-se|sf|...;...", or a list
 * of "service" hyphen "dependencies...".  In the above example, sa
 * depends on functions within sb, sc, and sn.
 */
int
deserialize_dependencies(char *deps, struct service_symbs *services)
{
	char *next, *current;
	char *serial         = "-";
	char *parallel       = "|";
	char  inter_dep      = ';';
	char  open_modifier  = '[';
	char  close_modifier = ']';

	if (!deps) return -1;
	next = current = deps;

	printf("deps %s\n", deps);
	if (strlen(current) == 0) return 0;
	/* go through each dependent-trusted|... relation */
	while (current) {
		struct service_symbs *s, *dep;
		char *                tmp;

		next = strchr(current, inter_dep);
		if (next) {
			*next = '\0';
			next++;
		}

		/* the dependent */
		tmp = strtok(current, serial);
		s   = get_service_struct(tmp, services);
		if (!s) {
			printl(PRINT_HIGH, "Could not find service %s.\n", tmp);
			return -1;
		}

		/* go through the | invoked services */
		tmp = strtok(NULL, parallel);
		while (tmp) {
			char *mod = NULL;
			/* modifier! */
			if (tmp[0] == open_modifier) {
				mod = tmp + 1;
				tmp = strchr(tmp, close_modifier);
				if (!tmp) {
					printl(PRINT_HIGH, "Could not find closing modifier ] in %s\n", mod);
					return -1;
				}
				*tmp = '\0';
				tmp++;
				assert(mod);
				assert(tmp);
			}
			dep = get_service_struct(tmp, services);
			if (!dep) {
				printl(PRINT_HIGH, "Could not find service %s.\n", tmp);
				return -1;
			}
			if (dep == s) {
				printl(PRINT_HIGH, "Reflexive relations not allowed (for %s).\n", s->obj);
				return -1;
			}

			if (!is_booter_loaded(s) && is_booter_loaded(dep)) {
				printl(PRINT_HIGH,
				       "Error: Non-Composite-loaded component %s dependent "
				       "on composite loaded component %s.\n",
				       s->obj, dep->obj);
				return -1;
			}

			if (mod)
				add_modified_service_dependency(s, dep, mod, strlen(mod));
			else
				add_service_dependency(s, dep);

			if (dep->is_scheduler) {
				if (NULL == s->scheduler) {
					s->scheduler = dep;
				} else if (dep != s->scheduler) {
					printl(PRINT_HIGH,
					       "Service %s is dependent on more than "
					       "one scheduler (at least %s and %s).  Error.\n",
					       s->obj, dep->obj, s->scheduler->obj);
					return -1;
				}
			}

			tmp = strtok(NULL, parallel);
		}

		current = next;
	}

	return 0;
}
