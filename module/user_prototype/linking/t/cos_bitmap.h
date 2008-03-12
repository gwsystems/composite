/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_BITMAP_H
#define COS_BITMAP_H

/* bitmap support: */

#define INT_SIZE 32
typedef struct {
	int n;
	unsigned int *bits;
} bitmap_t;

#define CREATE_STATIC_BITMAP(name, n_entries) \
unsigned int name##_bits[n_entries/INT_SIZE]; \
bitmap_t name = {.n = n_entries, .bits = name##_bits}

static inline int bit_is_set(bitmap_t *bm, unsigned int n_entry)
{
	unsigned int idx = n_entry>>5; /* same as n_entry/INT_SIZE */
	unsigned int mask = 1<<(n_entry & (INT_SIZE-1));

	if (idx >= bm->n) {
		return -1;
	}

	return mask & bm->bits[idx];
}

static inline void bit_set(bitmap_t *bm, unsigned int n_entry, int val)
{
	unsigned int idx = n_entry>>5;
	unsigned int mask = 1<<(n_entry & (INT_SIZE-1));

	/* silently terminate...perhaps not the most desired result */
	if (idx >= bm->n) {
		return;
	}

	if (val) {
		bm->bits[idx] |= mask;
	} else {
		bm->bits[idx] &= ~mask;
	}
	
	return;
}

#endif
