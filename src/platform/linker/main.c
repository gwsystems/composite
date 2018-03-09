/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Linker and loader for the Composite system: takes a collection of
 * services with their trust relationships explicitly expressed,
 * dynamically generates their stub code to connect them, communicates
 * capability information info with the runtime system, creates the
 * user-level static capability structures, and loads the services
 * into the current address space which will be used as a template for
 * the run-time system for creating each service protection domain
 * (ie. copying the entries in the pgd to new address spaces.
 *
 * This is trusted code, and any mistakes here compromise the entire
 * system.  Essentially, control flow is restricted/created here.
 *
 * Going by the man pages, I think I might be going to hell for using
 * strtok so much.  Suffice to say, don't multithread this program.
 */

#include "cl_types.h"
#include "cl_macros.h"
#include "cl_globals.h"

#include <stdio.h>
#include <string.h>

int                   deserialize_dependencies(char *deps, struct service_symbs *services);
void                  gen_stubs_and_link(char *gen_stub_prog, struct service_symbs *services);
unsigned long         load_all_services(struct service_symbs *services);
void                  print_objs_symbs(struct service_symbs *str);
struct service_symbs *prepare_service_symbs(char *services);
void                  output_image(struct service_symbs *services);
int                   verify_dependency_completeness(struct service_symbs *services);
int                   verify_dependency_soundness(struct service_symbs *services);

/*
 * Format of the input string is as such:
 *
 * "s1,s2,s3,...,sn:s2-s3|...|sm;s3-si|...|sj"
 *
 * Where the pre-: comma-separated list is simply a list of all
 * involved services.  Post-: is a list of truster (before -) ->
 * trustee (all trustees separated by '|'s) relations separated by
 * ';'s.
 */
int
main(int argc, char *argv[])
{
	struct service_symbs *services;
	char *                delim = ":";
	char *                servs, *dependencies = NULL, *ndeps = NULL, *stub_gen_prog;
	long                  service_addr;

	if (argc != 3) {
		printl(PRINT_HIGH,
		       "Usage: %s [-q] <comma separated string of all "
		       "objs:truster1-trustee1|trustee2|...;truster2-...> "
		       "<path to gen_client_stub>\n",
		       argv[0]);
		return 1;
	}

	stub_gen_prog = argv[2];

	/*
	 * NOTE: because strtok is used in prepare_service_symbs, we
	 * cannot use it relating to the command line args before AND
	 * after that invocation
	 */
	if (!strstr(argv[1], delim)) {
		printl(PRINT_HIGH, "No %s separating the component list from the dependencies in %s\n", delim, argv[1]);
		return 1;
	}

	/* find the last : */
	servs = strtok(argv[1], delim);
	while ((ndeps = strtok(NULL, delim))) {
		dependencies = ndeps;
		*(ndeps - 1) = ':';
	}
	if (!dependencies)
		dependencies = "";
	else
		*(dependencies - 1) = '\0';

	if (!servs) {
		printl(PRINT_HIGH, "You must specify at least one service.\n");
		return 1;
	}

	bfd_init();

	services = prepare_service_symbs(servs);

	print_objs_symbs(services);

	if (!dependencies) {
		printl(PRINT_HIGH, "No dependencies given, not proceeding.\n");
		return 1;
	}

	if (deserialize_dependencies(dependencies, services)) {
		printl(PRINT_HIGH, "Error processing dependencies.\n");
		return 1;
	}

	if (verify_dependency_completeness(services)) {
		printl(PRINT_HIGH, "Unresolved symbols, not linking.\n");
		return 1;
	}

	if (verify_dependency_soundness(services)) {
		printl(PRINT_HIGH, "Services arranged in an invalid configuration, not linking.\n");
		return 1;
	}

	gen_stubs_and_link(stub_gen_prog, services);
	service_addr = load_all_services(services);

	if (service_addr < 0) {
		printl(PRINT_HIGH, "Error loading services, aborting.\n");
		return 1;
	}

	printf("Service address: %08x\n", service_addr);

	output_image(services);

	return 0;
}
