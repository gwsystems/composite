/* 
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#include <tmem.h>
#include <cbuf_c.h>

/* 
 * For a certain principal, collect any unreferenced persistent cbufs
 * so that they can be reused.  This is the garbage-collection
 * mechanism.
 *
 * Collect cbufps and add them onto the component's freelist.
 */
int
cbufp_collect(struct spd_tmem_info *sti, int size)
{
	return -1;
}

extern struct cos_cbuf_item *alloc_item_data_struct(void *l_addr);
/* 
 * Get a new persistent cbuf of a specific size.
 */
struct cos_cbuf_item *
cbufp_grant(struct spd_tmem_info *sti, int size)
{
	struct cos_cbuf_item *cbi = NULL, *ncbi;

	assert(size <= PAGE_SIZE);

	return NULL;

	if (!sti->data) {
		if (cbufp_collect(sti, PAGE_SIZE)) {
			/* no memory collected... */
			void *p = alloc_page();
			assert(p);

			ncbi    = alloc_item_data_struct(p);
			assert(ncbi);
			ncbi->parent_spdid = ncbi->desc.cbid = ncbi->desc.owner.spd = sti->spdid;
			ncbi->desc.sz         = PAGE_SIZE;
			ncbi->desc.owner.addr = (vaddr_t)p;
			ncbi->desc.owner.cbd  = &ncbi->desc;
			INIT_LIST(&ncbi->desc.owner, next, prev);
		} else {
			cbi = sti->data;
			assert(cbi);
		}
	} else {
		cbi = sti->data;
	}

	if (EMPTY_LIST(cbi, next, prev)) sti->data = NULL;
	else                             sti->data = FIRST_LIST(cbi, next, prev);
	REM_LIST(cbi, next, prev);
	
	return NULL;
}

/* 
 * Return back to the main pool a cbuf.
 */
int
cbufp_return(struct spd_tmem_info *sti)
{
	return -1;
}
