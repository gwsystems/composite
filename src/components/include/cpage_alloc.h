/***
 * Copyright 2012 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 *
 * A simple page allocator and free list.  Dumbest possible memory
 * allocation.  Useful instead of a buddy allocator when you know that
 * all allocations will be PAGESIZE or less.
 */

#ifndef CPAGE_ALLOC_H
#define CPAGE_ALLOC_H

#include <cos_component.h>

#ifndef CPAGE_ALLOC
#error "You must define CPAGE_ALLOC"
#endif

struct free_page {
	struct free_page *next;
};
static struct free_page page_list = {.next = NULL};

static inline void *
cpage_alloc(void)
{
	struct free_page *fp;
	void *            a;

	fp = page_list.next;
	if (NULL == fp) {
		a = CPAGE_ALLOC();
	} else {
		page_list.next = fp->next;
		fp->next       = NULL;
		a              = (void *)fp;
	}

	return a;
}

static inline void
cpage_free(void *ptr)
{
	struct free_page *fp;

	fp             = (struct free_page *)ptr;
	fp->next       = page_list.next;
	page_list.next = fp;

	return;
}

#endif
