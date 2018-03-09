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

#include <stdio.h>
#include <string.h>

inline trans_cap_t
is_transparent_capability(struct symb *s, int *fltn)
{
	char *n = s->name;
	*fltn   = 0;

	if (s->modifier_offset) {
		printf("%s -> %s.\n", n, n + s->modifier_offset);
	}
	if (!strcmp(n, SCHED_CREATE_FN)) return TRANS_CAP_SCHED;
	if (-1 != (*fltn = fault_handler_num(n + s->modifier_offset))) return TRANS_CAP_FAULT;
	return TRANS_CAP_NIL;
}

trans_cap_t is_transparent_capability(struct symb *s, int *fltn);


int
create_transparent_capabilities(struct service_symbs *service)
{
	int                i, j, fault_found[COS_FLT_MAX], other_found = 0;
	struct dependency *dep = service->dependencies;

	memset(fault_found, 0, sizeof(int) * COS_FLT_MAX);

	for (i = 0; i < service->num_dependencies; i++) {
		struct symb_type *symbs    = &dep[i].dep->exported;
		char *            modifier = dep[i].modifier;
		int               mod_len  = dep[i].mod_len;

		for (j = 0; j < symbs->num_symbs; j++) {
			trans_cap_t r;
			int         fltn;

			r = is_transparent_capability(&symbs->symbs[j], &fltn);
			switch (r) {
			case TRANS_CAP_FAULT:
				if (fault_found[fltn]) break;
				fault_found[fltn] = 1;
			case TRANS_CAP_SCHED: {
				struct symb_type *st;
				struct symb *     s;
				char              mod_name[256]; /* arbitrary value... */

				//                              if (symb_already_undef(service, symbs->symbs[j].name))
				//                              break;
				if (r == TRANS_CAP_SCHED) {
					/* This is pretty crap: We
					 * don't want to ad a
					 * capability to a parent
					 * scheduler if we already
					 * have a capability
					 * established, but we _do_
					 * want to create a capability
					 * for other special
					 * symbols. */
					if (dep[i].resolved) continue;
					if (other_found) break;
					other_found = 1;
				}

				assert(strlen(symbs->symbs[j].name) + mod_len < 256);
				strncpy(mod_name, modifier, mod_len);
				mod_name[mod_len] = '\0';
				strcat(mod_name, symbs->symbs[j].name);

				add_undef_symb(service, mod_name, mod_len);
				st               = &service->undef;
				s                = &st->symbs[st->num_symbs - 1];
				s->exporter      = dep[i].dep;
				s->exported_symb = &symbs->symbs[j];

				// service->obj, s->exporter->obj, s->exported_symb->name);

				dep[i].resolved = 1;
				break;
			}
			case TRANS_CAP_NIL:
				break;
			}
		}
		if (!dep[i].resolved) {
			printl(PRINT_HIGH,
			       "Warning: dependency %s-%s "
			       "is not creating a capability.\n",
			       service->obj, dep[i].dep->obj);
		}
	}

	return 0;
}


/*
 * Find the exporter for a specific symbol from amongst a list of
 * exporters.
 */
inline struct service_symbs *
find_symbol_exporter_mark_resolved(struct symb *s, struct dependency *exporters, int num_exporters,
                                   struct symb **exported)
{
	int i, j;

	for (i = 0; i < num_exporters; i++) {
		struct dependency *exporter;
		struct symb_type * exp_symbs;

		exporter  = &exporters[i];
		exp_symbs = &exporter->dep->exported;

		for (j = 0; j < exp_symbs->num_symbs; j++) {
			if (!strcmp(s->name, exp_symbs->symbs[j].name)) {
				*exported             = &exp_symbs->symbs[j];
				exporters[i].resolved = 1;
				return exporters[i].dep;
			}
			if (exporter->modifier && !strncmp(s->name, exporter->modifier, exporter->mod_len)
			    && !strcmp(s->name + exporter->mod_len, exp_symbs->symbs[j].name)) {
				*exported             = &exp_symbs->symbs[j];
				exporters[i].resolved = 1;
				return exporters[i].dep;
			}
		}
	}

	return NULL;
}

struct service_symbs *find_symbol_exporter_mark_resolved(struct symb *s,
							 struct dependency *exporters,
							 int num_exporters, struct symb **exported);


/*
 * Verify that all symbols can be resolved by the present dependency
 * relations.
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
int
verify_dependency_completeness(struct service_symbs *services)
{
	struct service_symbs *start = services;
	int                   ret   = 0;
	int                   i;

	/* for each of the services... */
	for (; services; services = services->next) {
		struct symb_type *undef_symbs = &services->undef;

		/* ...go through each of its undefined symbols... */
		for (i = 0; i < undef_symbs->num_symbs; i++) {
			struct symb *         symb = &undef_symbs->symbs[i];
			struct symb *         exp_symb;
			struct service_symbs *exporter;

			/*
			 * ...and make sure they are matched to an
			 * exported function in a service we are
			 * dependent on.
			 */
			exporter = find_symbol_exporter_mark_resolved(symb, services->dependencies,
			                                              services->num_dependencies, &exp_symb);
			if (!exporter) {
				printl(PRINT_HIGH, "Could not find exporter of symbol %s in service %s.\n", symb->name,
				       services->obj);

				ret = -1;
				goto exit;
			} else {
				symb->exporter      = exporter;
				symb->exported_symb = exp_symb;
			}
			/* if (exporter->is_scheduler) { */
			/* 	if (NULL == services->scheduler) { */
			/* 		services->scheduler = exporter; */
			/* 		//printl(PRINT_HIGH, "%s has scheduler %s.\n", services->obj, exporter->obj); */
			/* 	} else if (exporter != services->scheduler) { */
			/* 		printl(PRINT_HIGH, "Service %s is dependent on more than one scheduler (at least
			 * %s and %s).  Error.\n", services->obj, exporter->obj, services->scheduler->obj); */
			/* 		ret = -1; */
			/* 		goto exit; */
			/* 	} */
			/* } */
		}
	}

	for (services = start; services; services = services->next) {
		create_transparent_capabilities(services);
	}
exit:
	return ret;
}
