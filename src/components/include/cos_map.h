/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_MAP_H
#define COS_MAP_H

#ifdef COS_LINUX_ENV
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#else
#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#endif

#define COS_MAP_DYNAMIC
#ifdef COS_MAP_DYNAMIC
#ifndef COS_VECT_DYNAMIC
#define COS_VECT_DYNAMIC
#endif
#endif
#include <cos_vect.h>

/*
 * A map is one vector, that contains alternating slots for values and
 * freelist.  One to keep pointers to the data, and one to track the
 * free entries in the data vector.  Maps are optimized to add values
 * in general into an unspecified slot (they create the
 * index). Vectors are like arrays in that to find an arbitrary slot,
 * you have O(n).  This is the difference between the two.  In the
 * map, adding into a specific id is O(n).
 */
typedef struct cos_map_struct {
	cos_vect_t data;
	long       free_list, id_boundary;
} cos_map_t;

#ifndef COS_MAP_BASE
#define COS_MAP_BASE (COS_VECT_BASE / 2)
#endif

/* depth = 0 indicates that we haven't initialized the structure */
#define COS_MAP_CREATE_STATIC(name)                                   \
	struct cos_vect_intern_struct __##name##_vect[COS_VECT_BASE]; \
	cos_map_t                     name = {.data = {.depth = 0, .vect = __##name##_vect}, .free_list = 0}


static inline long
cos_vect_to_map_id(long vid)
{
	return vid / 2;
}
static inline long
cos_map_to_vect_id(long mid)
{
	return mid * 2;
}
static inline long
cos_map_to_vect_freeid(long mid)
{
	return cos_map_to_vect_id(mid) + 1;
}
static inline struct cos_vect_intern_struct *
cos_val_to_free(struct cos_vect_intern_struct *val)
{
	return val + 1;
}
static inline struct cos_vect_intern_struct *
cos_free_to_val(struct cos_vect_intern_struct *f)
{
	return f - 1;
}

static inline void
__cos_map_init(cos_map_t *m)
{
	int i;

	assert(m);
	if (__cos_vect_init(&m->data)) BUG();
	m->free_list   = 0;
	m->id_boundary = COS_MAP_BASE;
	/* Create the freelist */
	for (i = 0; i < (int)COS_MAP_BASE; i++) {
		int j = cos_map_to_vect_freeid(i);
		if (__cos_vect_set(&m->data, j, (void *)(i + 1))) BUG();
	}
	/* The end of the freelist */
	if (__cos_vect_set(&m->data, cos_map_to_vect_freeid(COS_MAP_BASE - 1), (void *)-1)) BUG();
}

static inline void
cos_map_init_static(cos_map_t *m)
{
	__cos_map_init(m);
}

static inline void
cos_map_init(cos_map_t *m)
{
	__cos_map_init(m);
}

#ifdef COS_MAP_DYNAMIC
#ifdef COS_LINUX_ENV
#include <malloc.h>
#else
#include <cos_alloc.h>
#endif /* COS_LINUX_ENV */


static cos_map_t *
cos_map_alloc_map()
{
	cos_map_t *m;

	m = malloc(sizeof(cos_map_t));
	if (NULL == m) goto err;

	if (cos_vect_alloc_vect_data(&m->data)) goto err_free_map;
	cos_map_init(m);

	return m;

err_free_map:
	free(m);
err:
	return NULL;
}

static void
cos_map_free_map(cos_map_t *m)
{
	assert(m);
	COS_VECT_FREE(m->data.vect);
	free(m);
}

#endif /* COS_MAP_DYNAMIC */

static inline void *
cos_map_lookup(cos_map_t *m, long mid)
{
	long vid = cos_map_to_vect_id(mid);

	return cos_vect_lookup(&m->data, vid);
}

/* return the id of the value */
static inline long
cos_map_add(cos_map_t *m, void *val)
{
	long                           free;
	struct cos_vect_intern_struct *is_free, *is;

	assert(m);
	free = m->free_list;
	/* no free slots? Create more! */
	if (free == -1) {
		long lower, upper;
		if (val != COS_VECT_INIT_VAL && 0 > cos_vect_add_id(&m->data, val, cos_map_to_vect_id(m->id_boundary)))
			return -1;
		free = lower   = m->id_boundary;
		m->id_boundary = upper = lower + COS_MAP_BASE;
		/* Add the new values to the free list */
		while (lower != upper) {
			int idx = cos_map_to_vect_freeid(lower);
			if (__cos_vect_set(&m->data, idx, (void *)(lower + 1))) BUG();
			lower++;
		}
		/* The end of the freelist */
		if (__cos_vect_set(&m->data, cos_map_to_vect_freeid(upper - 1), (void *)-1)) BUG();
	}

	is = __cos_vect_lookup(&m->data, cos_map_to_vect_id(free));
	assert(NULL != is);
	is->val = val;

	is_free      = cos_val_to_free(is);
	m->free_list = (long)is_free->val;
	is_free->val = (void *)-1;

	return free;
}

/*
 * This function will try to find an empty slot specifically for the
 * identifier id, or fail.
 *
 * This is O(n), n = MAX_EVENTS.  If we make the cos_vect_intern_struct
 * have a doubly linked free list, this would go away, but I don't
 * want the memory consumption.
 *
 * If you don't O(n), then just use a vect, not a map.  Map is for
 * cos_map_add.
 */
static inline long
cos_map_add_id(cos_map_t *m, void *val, long mid)
{
	struct cos_vect_intern_struct *is;
	long                           next, prev;

	/* not really tested enough.  This is not the purpose of map,
	 * use vect instead */
	BUG();

	/* All of this to maintain the free list... */
	assert(m);
	prev = next = m->free_list;
	is          = __cos_vect_lookup(&m->data, cos_map_to_vect_freeid(next));
	if (NULL == is) return -1;
	if (next == mid) {
		struct cos_vect_intern_struct *data_is;

		data_is = cos_free_to_val(is);
		assert(data_is);
		m->free_list = (long)is->val;
		data_is->val = val;
		is->val      = (void *)-1;
		return mid;
	}
	next = (long)is->val;
	while (-1 != next) {
		is = __cos_vect_lookup(&m->data, cos_map_to_vect_freeid(next));
		assert(is);
		next = (long)is->val;
		if (next == mid) {
			struct cos_vect_intern_struct *prev_is, *data_is;

			prev_is = __cos_vect_lookup(&m->data, cos_map_to_vect_freeid(prev));
			data_is = cos_free_to_val(is);
			assert(prev_is && data_is);
			/* remove from free list */
			prev_is->val = is->val;
			data_is->val = val;
			is->val      = (void *)-1;

			return mid;
		}
		prev = next;
		next = (long)is->val;
	}
	return -1;
}

static inline int
cos_map_del(cos_map_t *m, long mid)
{
	struct cos_vect_intern_struct *is;

	assert(m);
	is = __cos_vect_lookup(&m->data, cos_map_to_vect_id(mid));
	if (NULL == is) return -1;
	is->val = COS_VECT_INIT_VAL;
	is      = __cos_vect_lookup(&m->data, cos_map_to_vect_freeid(mid));
	assert(NULL != is);
	is->val      = (void *)m->free_list;
	m->free_list = mid;
	return 0;
}

#endif /* COS_MAP_H */
