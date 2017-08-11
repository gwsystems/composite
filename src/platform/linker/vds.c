/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 */

#include "cl_types.h"
#include "cl_macros.h"
#include "cl_globals.h"

#include <stdio.h>

static int
rec_verify_dag(struct service_symbs *services, int current_depth, int max_depth)
{
	int i;

	/* cycle */
	if (current_depth > max_depth) {
		return -1;
	}

	if (current_depth > services->depth) {
		services->depth = current_depth;
	}

	for (i = 0; i < services->num_dependencies; i++) {
		struct service_symbs *d = services->dependencies[i].dep;

		if (rec_verify_dag(d, current_depth + 1, max_depth)) {
			printl(PRINT_HIGH, "Component %s found in cycle\n", d->obj);
			return -1;
		}
	}

	return 0;
}


/*
 * FIXME: does not check for disjoint graphs at this time.
 *
 * The only soundness we can really check here is that services are
 * arranged in a DAG, i.e. that no cycles exist.  O(N^2*E).
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
int
verify_dependency_soundness(struct service_symbs *services)
{
	struct service_symbs *tmp_s = services;
	int                   cnt   = 0;

	while (tmp_s) {
		cnt++;
		tmp_s = tmp_s->next;
	}

	while (services) {
		if (rec_verify_dag(services, 0, cnt)) {
			printl(PRINT_DEBUG, "Cycle found in dependencies.  Not linking.\n");
			return -1;
		}

		services = services->next;
	}

	return 0;
}
