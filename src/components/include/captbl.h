/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * This is a capability table implementation based on embedded radix
 * trees.
 */

#ifndef CAPTBL_H
#define CAPTBL_H

#include <ertrie.h>
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE  64
#define CACHELINE_ORDER 6
#endif

#define CAPTBL_DEPTH    2
#define CAPTBL_INTERNSZ sizeof(int*)
#define CAPTBL_LEAFSZ   sizeof(struct cap_min)
typedef enum {
	CAP_FREE = 0,
	CAP_SCOMM,		/* synchronous communication */
	CAP_ASCOMM_SND,		/* async communication; sender */
	CAP_ASCOMM_RCV,         /* async communication; receiver */
	CAP_THD,                /* thread */
} cap_t;
/* 
 * The values in this enum are the order of the size of the
 * capabilities in this cacheline, offset by CAP_SZ_OFF (to compress
 * memory).
 */
typedef enum {
	CAP_SZ_16B = 0,
	CAP_SZ_32B = 1,
	CAP_SZ_64B = 2,
	CAP_SZ_OFF = 4,
} cap_sz_t;
typedef enum {
	CAP_FLAG_RO    = 1,
	CAP_FLAG_LOCAL = 1<<1,
} cap_flags_t;

#define CAP_HEAD_SZ_SZ    2
#define CAP_HEAD_FLAGS_SZ 2
#define CAP_HEAD_AMAP_SZ  4
#define CAP_HEAD_TYPE_SZ  8

/* 
 * This is the header for each capability.  Includes information about
 * allowed operations (read/write for specific cores), the size of the
 * capabilities in a cache-line, and the type of the capability.
 */
struct cap_header {
	/* 
	 * Size is only populated on cache-line-aligned entries.
	 * Applies to all caps in that cache-line 
	 */
	cap_sz_t    size  : CAP_HEAD_SZ_SZ; 	
	cap_flags_t flags : CAP_HEAD_FLAGS_SZ;
	u8_t        amap  : CAP_HEAD_AMAP_SZ; 	/* allocation map */
	cap_t       type  : CAP_HEAD_TYPE_SZ;
	u16_t       poly;
} __attribute__((packed));

struct cap_min {
	struct cap_header h;
	char padding[(4*sizeof(int))-sizeof(struct cap_header)];
};

static void *
__captbl_allocfn(void *d, int sz, int last_lvl)
{
	(void)last_lvl;
	void **mem = d; 	/* really a pointer to a pointer */
	void *m    = *mem;
	assert(sz <= PAGE_SIZE/2);
	*mem = NULL;		/* NULL so we don't do mult allocs */

	return m;
}

#define CT_MSK(v, o) ((unsigned long)(v) & ~((1<<(o))-1))

static void 
ert_definit(struct ert_intern *a, int leaf)
{ 
	(void)a;
	struct cap_header *p;
	
	if (!leaf) {
		a->next = NULL; 
	} else {
		p        = (struct cap_header *)a;
		p->size  = CAP_SZ_64B;
		p->type  = CAP_FREE;
		p->amap  = 0;
		p->flags = 0;
	}
}

static inline void *
__captbl_getleaf(struct ert_intern *a, void *accum)
{ 
	(void)accum;
	struct cap_header *p = (struct cap_header *)CT_MSK(a, CACHELINE_ORDER);
	struct cap_header *c = (struct cap_header *)CT_MSK(a, p->size + CAP_SZ_OFF);
	/* 
	 * We could do error checking here to make sure that a == c,
	 * if we didn't want to avoid the extra branches:
	 * if (unlikely(a == (void*)c)) return NULL;
	 */
	
	return c; 
}

static inline void __captbl_setleaf(struct ert_intern *a, void *data)
{ (void)a; (void)data; assert(0); }

ERT_CREATE(__captbl, captbl, CAPTBL_DEPTH,				\
	   9 /* PAGE_SIZE/(2*(CAPTBL_DEPTH-1)*CAPTBL_INTERNSZ) */, CAPTBL_INTERNSZ, \
	   7 /*PAGE_SIZE/(2*CAPTBL_LEAFSZ) */, CAPTBL_LEAFSZ, \
	   NULL, __captbl_init, ert_defget, ert_defisnull, ert_defset,  \
	   __captbl_allocfn, __captbl_setleaf, __captbl_getleaf, ert_defresolve); 

static struct captbl *captbl_alloc(void *mem) { return __captbl_alloc(&mem); }

/* 
 * This function is the fast-path used for capability lookup in the
 * invocation path.
 */
static inline struct cap_header *
captbl_lkup(struct captbl *t, unsigned long cap)
{ 
	if (unlikely(cap >= __captbl_maxid())) return NULL;
	return __captbl_lkupan(t, cap, CAPTBL_DEPTH+1, NULL); 
}

static inline int
__captbl_store(unsigned long *addr, unsigned long new, unsigned long old)
{ (void)old; *addr = new; return 0; }
#define CTSTORE(a, n, o) __captbl_store((unsigned long *)a, (unsigned long)n, (unsigned long)o)
#define cos_throw(label, errno) { ret = (errno); goto label; }

static inline struct cap_header *
captbl_add(struct captbl *t, unsigned long cap, cap_sz_t sz, int *retval)
{ 
	struct cap_header *p, *h;
	struct cap_header l, o;
	int ret = 0, off;
	unsigned int mask = 0;

	if (unlikely(cap >= __captbl_maxid())) cos_throw(err, -EINVAL);
	p = __captbl_lkupan(t, cap, CAPTBL_DEPTH, NULL); 
	if (unlikely(!p)) cos_throw(err, -EPERM);
	h = (struct cap_header *)CT_MSK(p, CACHELINE_ORDER);
	l = o = *h;
	off = (struct cap_min*)p - (struct cap_min*)h; /* ptr math */

	if (unlikely(l.flags & CAP_FLAG_RO)) cos_throw(err, -EPERM);
	assert(off >= 0);
	if (unlikely(l.amap & (1<<off))) cos_throw(err, -EEXIST);
	if (unlikely(l.size < sz || (l.amap && (l.size != sz)))) cos_throw(err, -EEXIST);

	l.amap |= 1<<off;
	if (l.size != sz) {
		assert(l.size > sz);
		l.size = sz;
	}
	if (CTSTORE(h, o, l)) cos_throw(err, EEXIST); /* commit */
	assert(p->type == CAP_FREE);
	
	assert(p == __captbl_lkupan(t, cap, CAPTBL_DEPTH+1, NULL));
	return p;
err:
	*retval = ret;
	return NULL;
}
	
static inline int
captbl_del(struct captbl *t, unsigned long cap)
{
	struct cap_header *p, *h;
	struct cap_header l, o;
	int ret = 0, off;
	unsigned int mask = 0;

	if (unlikely(cap >= __captbl_maxid())) cos_throw(err, -EINVAL);
	p = __captbl_lkupan(t, cap, CAPTBL_DEPTH, NULL); 
	if (unlikely(!p)) cos_throw(err, -EPERM);
	if (p != __captbl_getleaf(p, NULL)) cos_throw(err, -EINVAL);

	h = (struct cap_header *)CT_MSK(p, CACHELINE_ORDER);
	l = o = *h;
	off = (struct cap_min*)p - (struct cap_min*)h;

	/* Do we want RO to prevent deletions? */
	if (unlikely(l.flags & CAP_FLAG_RO)) cos_throw(err, -EPERM);
	assert(off >= 0);
	if (unlikely(!(l.amap & (1<<off)) || l.type == CAP_FREE)) cos_throw(err, -ENOENT);

	l.amap &= (~(1<<off)) & ((1<<CAP_HEAD_AMAP_SZ)-1);
	if (l.amap == 0) l.size = CAP_SZ_64B; /* no active allocations... */
	if (CTSTORE(h, o, l)) cos_throw(err, -EEXIST); /* commit */
	if (p != h) p->type = CAP_FREE;
err:
	return ret;
}

static inline int
captbl_expand(struct captbl *t, unsigned long cap, void *memctxt)
{
	int ret;

	if (unlikely(cap > __captbl_maxid())) return -EINVAL;
	ret = __captbl_expand(t, cap, NULL, &memctxt, NULL);
	if (unlikely(ret)) return -EPERM;
	if (memctxt)       return -EEXIST;

	return 0;
}

/* 
 * Shrink down the capability tree to the specified depth along the
 * path to the specified capability number.
 */
static void *
captbl_shrink(struct captbl *t, unsigned long cap, u32_t depth)
{
	void **intern, *p;
	int ret;

	if (unlikely(cap   >= __captbl_maxid() || 
		     depth >= __captbl_maxdepth())) return -EINVAL;
	intern = __captbl_lkupan(t, cap, depth, NULL); 
	if (unlikely(!intern)) return -EPERM;
	p = *intern;
	if (CTSTORE(*intern, p, NULL)) cos_throw(err, -EEXIST); /* commit */

	return p;
}

#endif /* CAPTBL_H */
