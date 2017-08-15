/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CMAP_H
#define CMAP_H

#ifdef LINUX_TEST
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#else
#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#endif

#define CMAP_DYNAMIC
#ifdef CMAP_DYNAMIC
#ifndef CVECT_DYNAMIC
#define CVECT_DYNAMIC
#endif
#endif
#include <cvect.h>

/*
 * A map is one vector, that contains alternating slots for values and
 * freelist.  One to keep pointers to the data, and one to track the
 * free entries in the data vector.  Maps are optimized to add values
 * in general into an unspecified slot (they create the
 * index). Vectors are like arrays in that to find an arbitrary slot,
 * you have O(n).  This is the difference between the two.  In the
 * map, adding into a specific id is O(n).
 */
typedef struct cmap_struct {
	cvect_t *data;
	long     free_list, id_boundary;
} cmap_t;

/* depth = 0 indicates that we haven't initialized the structure */
#define CMAP_CREATE_STATIC(name)              \
	CVECT_CREATE_STATIC(__##name##_vect); \
	cmap_t name = {.data = &__##name##_vect, .free_list = -1, .id_boundary = 0}

static inline long
cvect_to_map_id(long vid)
{
	return vid / 2;
}
static inline long
cmap_to_vect_id(long mid)
{
	return mid * 2;
}
static inline long
cmap_to_vect_freeid(long mid)
{
	return cmap_to_vect_id(mid) + 1;
}
static inline struct cvect_intern *
cos_val_to_free(struct cvect_intern *val)
{
	return val + 1;
}
static inline struct cvect_intern *
cos_free_to_val(struct cvect_intern *f)
{
	return f - 1;
}

static inline void
__cmap_init(cmap_t *m)
{
	assert(m);
	m->data        = NULL;
	m->free_list   = -1;
	m->id_boundary = 0;
}

static inline void
cmap_init_static(cmap_t *m)
{
	return;
}
static inline void
cmap_init(cmap_t *m)
{
	__cmap_init(m);
}

#ifdef CMAP_DYNAMIC
#ifdef COS_LINUX_ENV
#include <malloc.h>
#else
#include <cos_alloc.h>
#endif /* COS_LINUX_ENV */

static cmap_t *
cmap_alloc(void)
{
	cmap_t *m;

	m = malloc(sizeof(cmap_t));
	if (NULL == m) goto err;
	cmap_init(m);

	m->data = cvect_alloc();
	if (!m->data) goto err_free_map;

	return m;
err_free_map:
	free(m);
err:
	return NULL;
}

static void
cmap_free(cmap_t *m)
{
	assert(m);
	cvect_free(m->data);
	free(m);
}

#endif /* CMAP_DYNAMIC */

static inline void *
cmap_lookup(cmap_t *m, long mid)
{
	return cvect_lookup(m->data, cmap_to_vect_id(mid));
}

/* return the id of the value */
static inline long
cmap_add(cmap_t *m, void *val)
{
	long                 free;
	struct cvect_intern *is_free, *is;

	assert(m);
	free = m->free_list;
	/* no free slots? Create more! */
	if (free == -1) {
		long lower, upper;
		if (val != CVECT_INIT_VAL && 0 > cvect_add_id(m->data, val, cmap_to_vect_id(m->id_boundary))) return -1;
		free = lower   = m->id_boundary;
		m->id_boundary = upper = lower + CVECT_BASE;
		/* Add the new values to the free list */
		while (lower != upper) {
			int idx = cmap_to_vect_freeid(lower);
			if (cvect_add(m->data, (void *)(lower + 1), idx)) assert(0);
			lower++;
		}
		/* The end of the freelist */
		if (__cvect_set(m->data, cmap_to_vect_freeid(upper - 1), (void *)-1)) assert(0);
	}

	is = __cvect_lookup(m->data, cmap_to_vect_id(free));
	assert(NULL != is);
	is->c.val = val;

	is_free        = cos_val_to_free(is);
	m->free_list   = (long)is_free->c.val;
	is_free->c.val = (void *)-1;

	return free;
}

/*
 * This function will try to find an empty slot specifically for the
 * identifier id, or fail.
 *
 * This is O(n), n = MAX_EVENTS.  If we make the cvect_intern
 * have a doubly linked free list, this would go away, but I don't
 * want the memory consumption.
 *
 * If you don't O(n), then just use a vect, not a map.  Map is for
 * cmap_add.
 */
static inline long
cmap_add_id(cmap_t *m, void *val, long mid)
{
	struct cvect_intern *is;
	long                 next, prev;

	/* not really tested enough.  This is not the purpose of map,
	 * use vect instead */
	assert(0);

	/* All of this to maintain the free list... */
	assert(m);
	prev = next = m->free_list;
	is          = __cvect_lookup(m->data, cmap_to_vect_freeid(next));
	if (NULL == is) return -1;
	if (next == mid) {
		struct cvect_intern *data_is;

		data_is = cos_free_to_val(is);
		assert(data_is);
		m->free_list   = (long)is->c.val;
		data_is->c.val = val;
		is->c.val      = (void *)-1;
		return mid;
	}
	next = (long)is->c.val;
	while (-1 != next) {
		is = __cvect_lookup(m->data, cmap_to_vect_freeid(next));
		assert(is);
		next = (long)is->c.val;
		if (next == mid) {
			struct cvect_intern *prev_is, *data_is;

			prev_is = __cvect_lookup(m->data, cmap_to_vect_freeid(prev));
			data_is = cos_free_to_val(is);
			assert(prev_is && data_is);
			/* remove from free list */
			prev_is->c.val = is->c.val;
			data_is->c.val = val;
			is->c.val      = (void *)-1;

			return mid;
		}
		prev = next;
		next = (long)is->c.val;
	}
	return -1;
}

static inline int
cmap_del(cmap_t *m, long mid)
{
	struct cvect_intern *is;

	assert(m && m->data);
	is = __cvect_lookup(m->data, cmap_to_vect_id(mid));
	if (NULL == is) return -1;
	is->c.val = CVECT_INIT_VAL;
	is        = cos_val_to_free(is);
	assert(NULL != is);
	is->c.val    = (void *)m->free_list;
	m->free_list = mid;
	return 0;
}

#endif /* CMAP_H */
