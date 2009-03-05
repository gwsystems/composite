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

struct cos_map_intern_struct {
	long free;
	void *val;
};

typedef struct cos_map_struct {
	unsigned short int depth, base; /* If we turn into a tree,
					 * what branching level? */
	long freelist;
	struct cos_map_intern_struct *map; /* This will actually be an array */
} cos_map_t;

/* depth = 0 indicates that we haven't initialized the structure */
#define COS_MAP_STATIC_CREATE(name, init_sz)      			\
	struct cos_map_intern_struct name##_map[ init_sz ];		\
	cos_map_t name = {.depth = 0, .base = init_sz, .freelist = -1, .map = name##_map}


static inline void cos_map_init_shared(cos_map_t *m, int base)
{
	struct cos_map_intern_struct *ms;
	int i;

	assert(m->depth == 0);
	m->depth = 1;
	m->freelist = 0;
	ms = m->map;
	m->base = base;

	for (i = 0 ; i < base ; i++) {
		ms[i].val = NULL;
		ms[i].free = i+1;
	}
	ms[base-1].free = -1;
}

static inline void cos_map_init_static(cos_map_t *m)
{
	cos_map_init_shared(m, m->base);
}

static inline void cos_map_init(cos_map_t *m, int init_sz)
{
	m->map = (struct cos_map_intern_struct*)&m[1];
	cos_map_init_shared(m, init_sz);
}

#ifdef COS_MAP_DYNAMIC
#ifdef COS_LINUX_ENV
#include <malloc.h>
#else
#include <cos_alloc.h>
#endif /* COS_LINUX_ENV */

static cos_map_t *cos_map_alloc_map(int init_sz)
{
	cos_map_t *m;
	
	m = malloc(sizeof(cos_map_t) + (init_sz) * sizeof(struct cos_map_intern_struct));
	if (NULL == m) return NULL;
	cos_map_init(m, init_sz);

	return m;
}

static void cos_map_free_map(cos_map_t *m)
{
	assert(m);
	free(m);
}

#endif /* COS_MAP_DYNAMIC */

static inline struct cos_map_intern_struct *cos_map_lookup_intern(cos_map_t *m, long id)
{
	assert(m && m->depth != 0);
	if (id >= m->base || id < 0) return NULL;

	return &m->map[id];
}
static inline void *cos_map_lookup(cos_map_t *m, long id)
{
	struct cos_map_intern_struct *is = cos_map_lookup_intern(m, id);

	if (NULL == is) return NULL;
	else            return is->val;
}

static inline long cos_map_add(cos_map_t *m, void *val)
{
	long free;
	struct cos_map_intern_struct *is;

	assert(m);
	free = m->freelist;
	if (free == -1) return -1;
	is = cos_map_lookup_intern(m, free);
	if (is == NULL) return -1;
	m->freelist = is->free;
	is->free = -1;
	is->val = val;

	return free;
}

static inline int cos_map_del(cos_map_t *m, long id)
{
	struct cos_map_intern_struct *is;

	assert(m);
	is = cos_map_lookup_intern(m, id);
	if (NULL == is) return 1;
	is->val = NULL;
	is->free = m->freelist;
	m->freelist = id;
	return 0;
}

#endif /* COS_MAP_H */
