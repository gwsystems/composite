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

struct cbufp_desc {
	struct cos_cbuf_item freelist;
};

/* 
 * Get a new persistent cbuf of a specific size.
 */
struct cos_cbuf_item *
cbufp_grant(struct spd_tmem_info *sti, int size)
{
	struct cbufp_desc *cbpd;
	struct cos_cbuf_item *cbi;

	return NULL;
	/* assert(size <= PAGE_SIZE); */

	/* if (!sti->data) { */
	/* 	cbpd = malloc(sizeof(struct cbufp_desc)); */
	/* 	if (!cbpd) return NULL; */
	/* } else { */
	/* 	cbpd = sti->data; */
	/* } */

	/* if (EMPTY_LIST(&cbpd->freelist, next, prev)) { */
	/* 	void *p; */

	/* 	/\*  */
	/* 	 * For now, we just allocate more memory! TODO: block */
	/* 	 * waiting for memory to become available on our free */
	/* 	 * list. */
	/* 	 *\/ */
	/* 	alloc_page( */
	/* } */
	/* cbi = FIRST_LIST(&cbpd->freelist, next, prev); */
	/* REM_LIST(cbi, next, prev); */
}

/* 
 * For a certain principal, collect any unreferenced persistent cbufs
 * so that they can be reused.  This is the garbage-collection
 * mechanism.
 */
int
cbufp_collect(struct spd_tmem_info *sti, int size)
{
	return -1;
}

/* 
 * Return back to the main pool a cbuf.
 */
int
cbufp_return(struct spd_tmem_info *sti)
{
	return -1;
}
