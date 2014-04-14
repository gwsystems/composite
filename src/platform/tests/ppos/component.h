/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COMPONENT_H
#define COMPONENT_H

struct comp_info {
	struct liveness_data liveness;
	pgtbl_t pgtbl;
	captbl_t captbl;
	struct cos_sched_data_area *comp_nfo;
} __attribute__((packed));

struct cap_comp {
	struct cap_header h;
	vaddr_t entry_addr;
	struct comp_info info;
} __attribute__((packed));

void comp_init(void)
{ assert(sizeof(struct cap_comp) <= __captbl_cap2bytes(CAP_COMP)); }

#ifndef /* COMPONENT_H */
