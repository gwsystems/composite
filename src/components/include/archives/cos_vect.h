#ifndef COS_VECT_H
#define COS_VECT_H

#ifndef COS_VECT_DYNAMIC
#define COS_VECT_DYNAMIC 1
#endif

/*
 * A simple data structure that behaves like an array in term of
 * getting and setting, but is O(log(n)) with a base that is chose
 * below.  In most situations this will be O(log_1024(n)), or
 * essentially at most 3.
 */

#ifdef COS_LINUX_ENV
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
#else
#include <cos_component.h>
#endif

struct cos_vect_intern_struct {
	void *val;
};

typedef struct cos_vect_struct {
	u16_t                          depth;
	struct cos_vect_intern_struct *vect;
} cos_vect_t;

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif
#define COS_VECT_PAGE_BASE (PAGE_SIZE / sizeof(struct cos_vect_intern_struct))
#ifndef COS_VECT_SHIFT
#define COS_VECT_SHIFT (PAGE_SHIFT - 2) /* -2 for pow2sizeof struct cos_vect_intern_struct */
#define COS_VECT_MASK (COS_VECT_PAGE_BASE - 1)
#endif

/* These three values to be overridden by user-level if inappropriate */
#ifndef COS_VECT_BASE
#define COS_VECT_BASE COS_VECT_PAGE_BASE
#define COS_VECT_SZ (COS_VECT_PAGE_BASE * sizeof(struct cos_vect_intern_struct))
#endif
#ifndef COS_VECT_INIT_VAL
#define COS_VECT_INIT_VAL NULL
#endif
#ifndef COS_VECT_DEPTH_MAX
#define COS_VECT_DEPTH_MAX 2
#endif

#define COS_VECT_CREATE_STATIC(name)                                         \
	struct cos_vect_intern_struct __cos_##name##_vect[COS_VECT_BASE] = { \
	  {.val = COS_VECT_INIT_VAL},                                        \
	};                                                                   \
	cos_vect_t name = {.depth = 1, .vect = __cos_##name##_vect}

/* true or false: is v a power of 2 */
static inline int
cos_vect_power_2(u32_t v)
{
	/* Assume 2's complement */
	u32_t smallest_set_bit = (v & -v);
	return (v > 1 && smallest_set_bit == v);
}

static inline int
__cos_vect_init(cos_vect_t *v)
{
	struct cos_vect_intern_struct *vs;
	int                            i;

	v->depth = 1;
	assert(cos_vect_power_2(COS_VECT_BASE));
	vs = v->vect;
	assert(vs);
	for (i = 0; i < (int)COS_VECT_BASE; i++) {
		vs[i].val = (void *)COS_VECT_INIT_VAL;
	}

	return 0;
}

static inline void
cos_vect_init_static(cos_vect_t *v)
{
	__cos_vect_init(v);
}

static inline void
cos_vect_init(cos_vect_t *v, struct cos_vect_intern_struct *vs)
{
	v->vect = vs;
	__cos_vect_init(v);
}

#ifdef COS_VECT_DYNAMIC
#ifdef COS_LINUX_ENV
#include <malloc.h>
#ifndef COS_VECT_ALLOC
#define COS_VECT_ALLOC malloc
#define COS_VECT_FREE free
#endif /* COS_VECT_ALLOC */
#else  /* COS_LINUX_ENV */
#include <cos_alloc.h>
#ifndef COS_VECT_ALLOC
#define COS_VECT_ALLOC(x) alloc_page()
#define COS_VECT_FREE free_page
#endif /* COS_VECT_ALLOC */
#endif /* COS_LINUX_ENV */


static int
cos_vect_alloc_vect_data(cos_vect_t *v)
{
	struct cos_vect_intern_struct *is;

	is = COS_VECT_ALLOC(sizeof(struct cos_vect_intern_struct) * COS_VECT_BASE);
	if (NULL == is) return -1;
	v->vect = is;
	return 0;
}

static cos_vect_t *
cos_vect_alloc_vect(void)
{
	cos_vect_t *v;

	v = malloc(sizeof(cos_vect_t));
	if (NULL == v) return NULL;

	if (cos_vect_alloc_vect_data(v)) {
		free(v);
		return NULL;
	}
	cos_vect_init(v, v->vect);

	return v;
}

static void
cos_vect_free_vect(cos_vect_t *v)
{
	assert(v && v->vect);
	COS_VECT_FREE(v->vect);
	free(v);
}

#endif /* COS_VECT_DYNAMIC */

static inline struct cos_vect_intern_struct *
__cos_vect_lookup(cos_vect_t *v, long id)
{
	long                           depth;
	struct cos_vect_intern_struct *is;

	/* make sure the data structure is configured and initialized */
	assert(v);
	assert(v->depth != 0);
	assert(v->depth <= 2);
	if (id < 0) return NULL;
	depth = v->depth;
#if COS_VECT_DEPTH_MAX != 2
#error "__cos_vect_lookup assumes a max depth of 2"
#endif
	is = v->vect;
	switch (depth) {
	case 2: {
		long t = (id >> COS_VECT_SHIFT);

		if (t >= (long)COS_VECT_BASE) return NULL;
		is = (struct cos_vect_intern_struct *)is[t & COS_VECT_MASK].val;
		if (NULL == is) return NULL;
		id &= COS_VECT_MASK;
		/* fallthrough */
	}
	case 1:
		if (id >= (long)COS_VECT_BASE) return NULL;
	}
	return &is[id];
}

static inline void *
cos_vect_lookup(cos_vect_t *v, long id)
{
	struct cos_vect_intern_struct *is = __cos_vect_lookup(v, id);

	if (NULL == is)
		return NULL;
	else
		return is->val;
}

static inline int
__cos_vect_expand(cos_vect_t *v, long id)
{
	struct cos_vect_intern_struct *is, *root;
	int                            i;

	assert(v && NULL == __cos_vect_lookup(v, id));

	/* do we want an index outside of the range of the current structure? */
	if (id >= (1 << (COS_VECT_SHIFT * v->depth))) {
		if (v->depth >= COS_VECT_DEPTH_MAX) return -1;

		is = COS_VECT_ALLOC(COS_VECT_BASE * sizeof(struct cos_vect_intern_struct));
		if (NULL == is) return -1;
		for (i = 0; i < (int)COS_VECT_BASE; i++) is[i].val = NULL;
		is->val = v->vect;
		v->depth++;
		v->vect = is;
	}

	/* we must be asking for an index that doesn't have a complete
	 * path through the tree (intermediate nodes) */
	assert(v->depth == 2);

	is = COS_VECT_ALLOC(COS_VECT_BASE * sizeof(struct cos_vect_intern_struct));
	if (NULL == is) return -1;
	for (i = 0; i < (int)COS_VECT_BASE; i++) is[i].val = (void *)COS_VECT_INIT_VAL;

	root = &v->vect[(id >> COS_VECT_SHIFT) & COS_VECT_MASK];
	assert(NULL == root->val);
	root->val = is;

	return 0;
}

static inline int
__cos_vect_set(cos_vect_t *v, long id, void *val)
{
	struct cos_vect_intern_struct *is = __cos_vect_lookup(v, id);

	if (NULL == is) return -1;
	is->val = val;

	return 0;
}

/*
 * This function will try to find an empty slot specifically for the
 * identifier id, or fail.
 */
static long
cos_vect_add_id(cos_vect_t *v, void *val, long id)
{
	struct cos_vect_intern_struct *is;

	assert(v && val != 0);
	is = __cos_vect_lookup(v, id);
	if (NULL == is) {
		if (__cos_vect_expand(v, id)) return -1;
		is = __cos_vect_lookup(v, id);
		assert(is);
	}
	is->val = val;

	return id;
}

static int
cos_vect_del(cos_vect_t *v, long id)
{
	assert(v);
	if (__cos_vect_set(v, id, (void *)COS_VECT_INIT_VAL)) return 1;
	return 0;
}


#endif /* COS_VECT_H */
