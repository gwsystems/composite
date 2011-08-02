#ifndef CBUF_VECT_H
#define CBUF_VECT_H

#ifndef CBUF_VECT_DYNAMIC
#define CBUF_VECT_DYNAMIC 1
#endif

/* 
 * A simple data structure that behaves like an array in term of
 * getting and setting, but is O(log(n)) with a base that is chose
 * below.  In most situations this will be O(log_1024(n)), or
 * essentially at most 3.
 */
#include <cbuf_c.h>

#ifdef COS_LINUX_ENV
typedef unsigned short int u16_t;
typedef unsigned int u32_t;
#else
#include <cos_component.h>
#endif

vaddr_t cbuf_c_register(spdid_t spdid, long cbid);


struct cbuf_vect_intern_struct {
	void *val;
};

typedef struct cbuf_vect_struct {
	u16_t depth;
	struct cbuf_vect_intern_struct *vect;
} cbuf_vect_t;

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif
#define CBUF_VECT_PAGE_BASE (PAGE_SIZE/sizeof(struct cbuf_vect_intern_struct))
#ifndef CBUF_VECT_SHIFT
#define CBUF_VECT_SHIFT (PAGE_SHIFT-2) /* -2 for pow2sizeof struct cbuf_vect_intern_struct */
#define CBUF_VECT_MASK  (CBUF_VECT_PAGE_BASE-1)
#endif

/* These three values to be overridden by user-level if inappropriate */
#ifndef CBUF_VECT_BASE
#define CBUF_VECT_BASE CBUF_VECT_PAGE_BASE
#define CBUF_VECT_SZ   (CBUF_VECT_PAGE_BASE*sizeof(struct cbuf_vect_intern_struct))
#endif
#ifndef CBUF_VECT_INIT_VAL
#define CBUF_VECT_INIT_VAL NULL
#endif
#ifndef CBUF_VECT_DEPTH_MAX
#define CBUF_VECT_DEPTH_MAX 2
#endif

#define CBUF_VECT_CREATE_STATIC(name)					\
	cbuf_vect_t name = {.depth = 0, .vect = NULL}
	/* struct cbuf_vect_intern_struct __cos_##name##_vect[ CBUF_VECT_BASE ] = {{.val=CBUF_VECT_INIT_VAL},}; \ */
	/* cbuf_vect_t name = {.depth = 1, .vect = __cos_##name##_vect} */

          

/* true or false: is v a power of 2 */
static inline int cbuf_vect_power_2(u32_t v)
{
	/* Assume 2's complement */
	u32_t smallest_set_bit = (v & -v);
	return (v > 1 && smallest_set_bit == v);
}

/* static inline int __cbuf_vect_init(cbuf_vect_t *v) */
/* { */
/* 	struct cbuf_vect_intern_struct *vs; */
/* 	int i; */

/* 	v->depth = 1; */
/* 	assert(cbuf_vect_power_2(CBUF_VECT_BASE)); */
/* 	vs = v->vect; */
/* 	assert(vs); */
/* 	for (i = 0 ; i < (int)CBUF_VECT_BASE ; i++) { */
/* 		vs[i].val = (void*)CBUF_VECT_INIT_VAL; */
/* 	} */

/* 	return 0; */
/* } */

/* static inline void cbuf_vect_init_static(cbuf_vect_t *v) */
/* { */
/* 	__cbuf_vect_init(v); */
/* } */

/* static inline void cbuf_vect_init(cbuf_vect_t *v, struct cbuf_vect_intern_struct *vs) */
/* { */
/* 	v->vect = vs; */
/* 	__cbuf_vect_init(v); */
/* } */

#ifdef CBUF_VECT_DYNAMIC

#ifdef COS_LINUX_ENV
#include <malloc.h>
#ifndef CBUF_VECT_ALLOC
#define CBUF_VECT_ALLOC malloc
#define CBUF_VECT_FREE  free
#endif /* CBUF_VECT_ALLOC */

#else  /* COS_LINUX_ENV */
#include <cos_alloc.h>
#ifndef CBUF_VECT_ALLOC
#define CBUF_VECT_ALLOC(x) alloc_page()
#define CBUF_VECT_FREE  free_page
#endif /* CBUF_VECT_ALLOC */
#endif /* COS_LINUX_ENV */


/* static int cbuf_vect_alloc_vect_data(cbuf_vect_t *v) */
/* { */
/* 	struct cbuf_vect_intern_struct *is; */

/* 	is = CBUF_VECT_ALLOC(sizeof(struct cbuf_vect_intern_struct) * CBUF_VECT_BASE); */
/* 	if (NULL == is) return -1; */
/* 	v->vect = is; */
/* 	return 0; */
/* } */

/* static cbuf_vect_t *cbuf_vect_alloc_vect(void) */
/* { */
/* 	cbuf_vect_t *v; */
	
/* 	v = malloc(sizeof(cbuf_vect_t)); */
/* 	if (NULL == v) return NULL; */
	
/* 	if (cbuf_vect_alloc_vect_data(v)) { */
/* 		free(v); */
/* 		return NULL; */
/* 	} */
/* 	cbuf_vect_init(v, v->vect); */

/* 	return v; */
/* } */

static void cbuf_vect_free_vect(cbuf_vect_t *v)
{
	assert(v && v->vect);
	CBUF_VECT_FREE(v->vect);
	free(v);
}

#endif /* CBUF_VECT_DYNAMIC */

static inline struct cbuf_vect_intern_struct *__cbuf_vect_lookup(cbuf_vect_t *v, long id)
{
	long depth;
	struct cbuf_vect_intern_struct *is;
	/* printc("id : %ld",id); */
	/* printc("cbuf_vect_v is %p depth is %ld vect is %p \n", v, v->depth, v->vect); */
	assert(v);
	assert(v->depth != 0);
	assert(v->depth <= 2);
	if (id < 0) return NULL;
	depth = v->depth;
#if CBUF_VECT_DEPTH_MAX != 2
#error "__cbuf_vect_lookup assumes a max depth of 2"
#endif
	is = v->vect;
	switch (depth) {
	case 2:
	{
		long t = (id >> CBUF_VECT_SHIFT);
		/* printc("id : %ld t: %ld\n ", id, t); */
		
		if (t >= (long)CBUF_VECT_BASE) return NULL;
		is = (struct cbuf_vect_intern_struct*)is[t & CBUF_VECT_MASK].val;
		if (NULL == is) return NULL;
		id &= CBUF_VECT_MASK;
		/* fallthrough */
	}
	case 1:
		if (id >= (long)CBUF_VECT_BASE) return NULL;
	}
	/* printc("id after mask: %ld\n",id); */
	/* int i; */
	/* for(i=1;i<20;i++) */
	/* 	printc("i:%d %p\n",i,&is[i]); */
	/* printc("return something\n"); */
	return &is[id];
}

static inline void *cbuf_vect_lookup(cbuf_vect_t *v, long id)
{
	struct cbuf_vect_intern_struct *is = __cbuf_vect_lookup(v, id);

	if (NULL == is) return NULL;
	else            return is->val;
}

static inline vaddr_t cbuf_vect_addr_lookup(cbuf_vect_t *v, long cbid)
{
	vaddr_t ret;
	ret = ((u32_t)cbuf_vect_lookup(v,cbid));
	if (ret){
		ret = ret << PAGE_ORDER;//& (~((1 << PAGE_ORDER)-1));
		return ret;
	}
	else{
		return NULL;
	}
}

static inline int __cbuf_vect_expand(cbuf_vect_t *v, long id, int f)
{
	struct cbuf_vect_intern_struct *is, *root;
	int i;

	// do it once for 2 levels
	if(unlikely(v->depth == 0)){

 		v->depth = 1;

		if (v->depth >= CBUF_VECT_DEPTH_MAX) return -1;

		printc("root in %ld!!\n",cos_spd_id());
		root = CBUF_VECT_ALLOC(CBUF_VECT_BASE * sizeof(struct cbuf_vect_intern_struct));
		if (NULL == root) return -1;
		printc("CBUF_VECT_BASE is %d\n",CBUF_VECT_BASE);
		
		for (i = 0 ; i < (int)CBUF_VECT_BASE ; i++) root[i].val = (void*)NULL;
		v->vect = root;
		/* TODO: test if this can be commented out: */
		/* is_0 = (struct cbuf_vect_intern_struct *)cbuf_c_register(cos_spd_id(),t); */
		/* if (NULL == is_0) return -1; */
		/* for (i = 0 ; i < (int)CBUF_VECT_BASE ; i++) is_0[i].val = (void*)CBUF_VECT_INIT_VAL; */
		v->depth++;
	}

	/* we must be asking for an index that doesn't have a complete
	 * path through the tree (intermediate nodes) */
	assert(v->depth == 2);

	assert(v && NULL == __cbuf_vect_lookup(v, id));

	if (f == 1)
		is = (struct cbuf_vect_intern_struct *)cbuf_c_register(cos_spd_id(), id);
	else
		is = CBUF_VECT_ALLOC(CBUF_VECT_BASE * sizeof(struct cbuf_vect_intern_struct));

	/* printc("extended: [[[[[[ %p ]]]]]]\n",is); */

	if (NULL == is) return -1;
	for (i = 0 ; i < (int)CBUF_VECT_BASE ; i++) is[i].val = (void*)CBUF_VECT_INIT_VAL;

	root = &v->vect[(id >> CBUF_VECT_SHIFT) & CBUF_VECT_MASK];
	assert(NULL == root->val);
	root->val = is;

	/* printc("cbuf_vect_t v is %p\n",v); */
	return 0;
}

static inline int cbuf_vect_expand(cbuf_vect_t *v, long id, int f) 
{ 
	return __cbuf_vect_expand(v, id, f);
}

static inline int __cbuf_vect_set(cbuf_vect_t *v, long id, void *val)
{
	struct cbuf_vect_intern_struct *is = __cbuf_vect_lookup(v, id);

	if (NULL == is) return -1;
	is->val = val;

	return 0;
}

/* 
 * This function will try to find an empty slot specifically for the
 * identifier id, or fail.
 */
static long cbuf_vect_add_id(cbuf_vect_t *v, void *val, long id)
{
	/* struct cbuf_vect_intern_struct *is_0, *is, *root; */
	struct cbuf_vect_intern_struct *is;

	/* assert(v && val != 0); */
	/* assert(v->depth == 2); */

	is = __cbuf_vect_lookup(v, id);

	/* if (NULL == is) { */
	/* 	printc("look up id %ld, going to expand!\n",id); */
	/* 	if (__cbuf_vect_expand(v, id)) return -1; */
	/* 	is = __cbuf_vect_lookup(v, id); */
	/* 	assert(is); */
	/* } */
	assert(is);

	is->val = val;

	return id;
}

static int cbuf_vect_del(cbuf_vect_t *v, long id)
{
	assert(v);
	if (__cbuf_vect_set(v, id, (void*)CBUF_VECT_INIT_VAL)) return 1;
	return 0;
}


#endif /* CBUF_VECT_H */
