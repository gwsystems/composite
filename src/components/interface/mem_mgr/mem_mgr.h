/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#ifndef   	MEM_MGR_H
#define   	MEM_MGR_H

int __mman_fork_spd(spdid_t spd, u32_t s_spd_d_spd, vaddr_t base, u32_t len);
static inline int
mman_fork_spd(spdid_t spd, spdid_t s_spd, spdid_t d_spd, vaddr_t base, u32_t len) {
	return __mman_fork_spd(spd, ((u32_t)s_spd<<16)|d_spd, base, len);
}

/* Map a physical frame into a component. */
vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags);
vaddr_t mman_valloc(spdid_t compid, spdid_t dest, unsigned long npages);

/* 
 * remove this single mapping _and_ all descendents.  FIXME: this can
 * be called with the spdid of a dependent component.  We should also
 * pass in the component id of the calling component to ensure that it
 * is allowed to remove the designated page.
 */
int mman_release_page(spdid_t spd, vaddr_t addr, int flags);
/* remove all descendent mappings of this one (but not this one). */ 
int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags); 
/* The invoking component (s_spd) must own the mapping. */
vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr);
static inline vaddr_t 
mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags)
{ return __mman_alias_page(s_spd, s_addr, ((u32_t)d_spd<<16)|flags, d_addr); }
void mman_print_stats(void);

#endif 	    /* !MEM_MGR_H */
