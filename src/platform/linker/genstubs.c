/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 */

#include "cl_types.h"
#include "cl_macros.h"
#include "cl_globals.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <libgen.h>

/*
 * Produces a number of object files in /tmp named objname.o.pid.o
 * with no external dependencies.
 *
 * gen_stub_prog is the address to the client stub generation prog
 * st_object is the address of the symmetric trust object.
 *
 * This is kind of a big hack.
 */
void
gen_stubs_and_link(char *gen_stub_prog, struct service_symbs *services)
{
	int  pid = getpid();
	char tmp_str[2048];

	while (services) {
		int               i;
		struct symb_type *symbs = &services->undef;
		char              dest[256];
		char              tmp_name[256];
		char *            obj_name, *orig_name, *str;

		orig_name = services->obj;
		obj_name  = basename(services->obj);
		sprintf(tmp_name, "/tmp/%s.%d", obj_name, pid);

		/*		if (symbs->num_symbs == 0) {
		                        sprintf(tmp_str, "cp %s %s.o",
		                                orig_name, tmp_name);
		                        system(tmp_str);

		                        str = malloc(strlen(tmp_name)+3);
		                        strcpy(str, tmp_name);
		                        strcat(str, ".o");
		                        free(services->obj);
		                        services->obj = str;

		                        services = services->next;
		                        continue;
		                }
		*/
		/* make the command line for an invoke the stub generator */
		strcpy(tmp_str, gen_stub_prog);

		if (symbs->num_symbs > 0) {
			strcat(tmp_str, " ");
			strcat(tmp_str, symbs->symbs[0].name);
		}
		for (i = 1; i < symbs->num_symbs; i++) {
			strcat(tmp_str, ",");
			strcat(tmp_str, symbs->symbs[i].name);
		}

		/* invoke the stub generator */
		sprintf(dest, " > %s_stub.S", tmp_name);
		strcat(tmp_str, dest);
		printl(PRINT_DEBUG, "%s\n", tmp_str);
		system(tmp_str);

		/* compile the stub */
		sprintf(tmp_str, GCC_BIN " -m32 -c -o %s_stub.o %s_stub.S", tmp_name, tmp_name);
		system(tmp_str);

		/* link the stub to the service */
		sprintf(tmp_str, LINKER_BIN " -m elf_i386 -r -o %s.o %s %s_stub.o", tmp_name, orig_name, tmp_name);
		system(tmp_str);

		/* Make service names reflect their new linked versions */
		str = malloc(strlen(tmp_name) + 3);
		strcpy(str, tmp_name);
		strcat(str, ".o");
		free(services->obj);
		services->obj = str;

		sprintf(tmp_str, "rm %s_stub.o %s_stub.S", tmp_name, tmp_name);
		system(tmp_str);

		services = services->next;
	}

	return;
}
