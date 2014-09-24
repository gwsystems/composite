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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void
output_image(struct service_symbs *services)
{
	struct service_symbs *s; //, *initial;
	u32_t entry_point = 0;
	char image_filename[18];
	int image;

	s = services;
	while (s) {
		printl(PRINT_DEBUG, "\n\nobj:                 '%s'\n", s->obj);
		printl(PRINT_DEBUG, "init_str:            '%s'\n", s->init_str);
		printl(PRINT_DEBUG, "lower_addr:          %08x\n", s->lower_addr);
		printl(PRINT_DEBUG, "size:                %08x\n", s->size);
		printl(PRINT_DEBUG, "allocated:           %08x\n", s->allocated);
		printl(PRINT_DEBUG, "heap_top:            %08x\n", s->heap_top);
		printl(PRINT_DEBUG, "mem_size:            %08x\n", s->mem_size);
        	//struct service_section sections[SERV_SECT_NUM];
		printl(PRINT_DEBUG, "is_composite_loaded: %d\n", s->is_composite_loaded);
		printl(PRINT_DEBUG, "already_loaded:      %d\n", s->already_loaded);
		printl(PRINT_DEBUG, "cobj:                %08x\n", s->cobj);
		printl(PRINT_DEBUG, "is_scheduler:        %d\n", s->is_scheduler);
        	//struct service_symbs *scheduler;
		//struct spd *spd;
        	//struct symb_type exported, undef;
		printl(PRINT_DEBUG, "num_dependencies:    %d\n", s->num_dependencies);
        	//struct dependency dependencies[MAX_TRUSTED];
        	//struct service_symbs *next;
		printl(PRINT_DEBUG, "depth:               %d\n", s->depth);
        	//void *extern_info;
		printl(PRINT_DEBUG, "\n");

		if (!strncmp(&s->obj[5], INIT_COMP, strlen(INIT_COMP))) {
			entry_point = get_symb_address(&s->exported, "spd0_main");
			sprintf(image_filename, "%08x-%08x", s->lower_addr, entry_point);
		} else {
			sprintf(image_filename, "%08x", s->lower_addr);
		}

		printl(PRINT_HIGH, "Writing image %s (%u bytes)\n", image_filename, s->mem_size);
		image = open(image_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (write(image, (void*)(s->cobj == 0 ? s->lower_addr : s->cobj), s->mem_size) < 0) {
			printl(PRINT_DEBUG, "Error number %d\n", errno);
			perror("Couldn't write image");
			exit(-1);
		}
		close(image);

		s = s->next;
	}

	return;
}
