/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* 
 * FIXME: locking!
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#define MAX_ALIASES 2

#define MEM_MARKED 1

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

static inline struct mem_cell *find_unused(void)
{
	int i, j;

	/* If we care about scaling, this should, of course use freelist */
	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		int free = 1;
		for (j = 0; j < MAX_ALIASES; j++) {
			cells[i].map[j].flags = 0;
			if (cells[i].map[j].owner_spd != 0) {
				free = 0;
				break;
			}
		}
		if (free) return &cells[i];
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

	c = find_unused();
	if (!c) {
		printc("mm: no more available pages!\n");
		goto err;
	}
	
	c->map[0].owner_spd = spd;
	c->map[0].addr = addr;

	/* Here we check for overwriting an already established mapping. */
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, spd, addr, cell_index(c))) {
		printc("mm: could not grant page @ %x to spd %d\n", addr, spd);
		c->map[0].owner_spd = 0;
		c->map[0].addr = 0;
		goto err;
	}

	return addr;
err:
	return 0;
}

/* 
 * Make an alias to a page in a source spd @ a source address to a
 * destination spd/addr
 */
vaddr_t mman_alias_page(spdid_t d_spd, vaddr_t d_addr, spdid_t s_spd, vaddr_t s_addr)
{
	int alias = -1, i;
	struct mem_cell *c;
	struct mapping_info *base;
	
	c = find_cell(s_spd, s_addr, &alias);
	assert(alias >= 0 && alias < MAX_ALIASES);
	base = c->map;
	for (i = 0 ; i < MAX_ALIASES ; i++) {
		if (base[i].owner_spd == 0 && base[i].addr == 0) {
			assert(i != alias);

			if (cos_mmap_cntl(COS_MMAP_GRANT, 0, d_spd, d_addr, cell_index(c))) {
				printc("mm: could not alias page @ %x to spd %d from %x(%d)\n", d_addr, d_spd, s_addr, s_spd);
				goto err;
			}
			base[i].owner_spd = d_spd;
			base[i].addr = d_addr;

			return d_addr;
		}
	}
	/* no available alias slots! */
err:
	return 0;
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

void mman_print_stats(void)
{
	int i, j, k, l;

	printc("Memory allocation stats:\n");
	for (k = 0 ; k < COS_MAX_MEMORY ; k++) {
		for (l = 0 ; l < MAX_ALIASES ; l++) {
			int spd_accum = 0, curr_spd;
			struct mapping_info *mc;

			mc = &cells[k].map[l];
			
			if (MEM_MARKED & mc->flags) continue;
			mc->flags |= MEM_MARKED;
			curr_spd = mc->owner_spd;
			spd_accum++;
			for (i = k ; i < COS_MAX_MEMORY ; i++) {
				for (j = 0 ; j < MAX_ALIASES ; j++) {
					mc = &cells[i].map[j];
					if (mc->owner_spd == curr_spd && !(MEM_MARKED & mc->flags)) {
						mc->flags |= MEM_MARKED;
						spd_accum++;
					}
				}
			}
			
			printc("\tspd %d used %d pages\n", curr_spd, spd_accum);
		}
	}
}

/* 
 * FIXME: add calls to obtain descriptors for the page regions, so
 * that they can be used to produce aliases.  This will allow for
 * shared memory, which we don't really support quite yet.
 */
