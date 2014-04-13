/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef THREAD_H
#define THREAD_H

#include <component.h>

struct invstk_entry {
	struct comp_info comp_info;
	unsigned long sp, ip; 	/* to return to */
};

/* TODO: replace with existing thread struct */
struct thread {
	int invstk_top;
	struct invstk_entry invstk[32] HALF_CACHE_ALIGNED;
};

struct cap_thd {
	struct cap_header h;
	struct thread *t;
	u32_t cpuid;
} __attribute__((packed));

void thd_init(void)
{ assert(sizeof(struct cap_comp) <= __captbl_cap2bytes(CAP_THD)); }

#ifndef /* THREAD_H */
