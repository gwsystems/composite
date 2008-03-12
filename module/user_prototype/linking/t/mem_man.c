/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>

struct mem_cell {
	unsigned short int owner_spd, flags;
	vaddr_t addr;
} __attribute__((packed));

static struct mem_cell cells[COS_MAX_MEMORY];

static inline long cell_index(struct mem_cell *c)
{
	return c - cells;
}

static inline struct mem_cell *find_unused(void)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		if (cells[i].owner_spd == 0) {
			return &cells[i];
		}
	}
	return NULL;
}

static inline struct mem_cell *find_cell(spdid_t spd, vaddr_t addr)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		struct mem_cell *c = &cells[i];

		if (c->owner_spd == spd && c->addr == addr) {
			return c;
		}
	}

	return NULL;
}

extern int print_vals(int, int, int, int);
/* 
 * Equivalent of mmap.
 */
vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mem_cell *c;

	c = find_unused();
	if (!c) {
		return 0;
	}
	
	c->owner_spd = spd;
	c->addr = addr;
		
	//print_vals(spd, addr, flags, 0);
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, spd, addr, cell_index(c))) {
		return 0;
	}

	return addr;
}

/*
 * Equivalent of munmap.
 */
void mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	int idx;

	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, spd, addr, 0);
	
	/* put the page back in the pool */
	cells[idx].owner_spd = 0;
	cells[idx].addr = 0;

	return;
}

void dump(void)
{
	print_vals(0,0,0,0);
}
