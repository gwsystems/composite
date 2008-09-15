/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <cos_debug.h>

#define MAX_ALIASES 2

struct mapping_info {
	unsigned short int owner_spd, flags;
	vaddr_t addr;
};
struct mem_cell {
	struct mapping_info map[MAX_ALIASES];
} __attribute__((packed));

static struct mem_cell cells[COS_MAX_MEMORY];

static inline long cell_index(struct mem_cell *c)
{
	return c - cells;
}

static inline struct mem_cell *find_unused(int *alias)
{
	int i, j;

	/* If we care about scaling, this should, of course use freelist */
	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		for (j = 0; j < MAX_ALIASES; j++) {
			if (cells[i].map[j].owner_spd == 0) {
				*alias = j;
				return &cells[i];
			}
		}
	}
	return NULL;
}

static inline struct mem_cell *find_cell(spdid_t spd, vaddr_t addr, int *alias)
{
	int i, j;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		struct mem_cell *c = &cells[i];

		for (j = 0; j < MAX_ALIASES; j++) {
			if (c->map[j].owner_spd == spd && 
			    c->map[j].addr == addr) {
				*alias = j;
				return c;
			}
		}
	}

	return NULL;
}

/* 
 * Call to get a page of memory at a location.
 */
vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mem_cell *c;
	int alias;

	c = find_unused(&alias);
	if (!c) {
		return 0;
	}
	
	c->map[alias].owner_spd = spd;
	c->map[alias].addr = addr;

	/* Here we check for overwriting an already established mapping. */
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, spd, addr, cell_index(c))) {
		return 0;
	}

	return addr;
}

/*
 * Call to give up a page of memory in an spd at an address.
 */
void mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	int idx, alias;
	struct mem_cell *mc;

	mc = find_cell(spd, addr, &alias);
	if (!mc) {
		/* FIXME: add return codes to this call */
		return;
	}
	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, spd, addr, 0);
	
	assert(&cells[idx] == mc);
	/* put the page back in the pool */
	mc->map[alias].owner_spd = 0;
	mc->map[alias].addr = 0;

	return;
}
