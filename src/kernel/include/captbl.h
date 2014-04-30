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

//#include <errno.h>
#include "ertrie.h"
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE  64
#define CACHELINE_ORDER 6
#endif

#define CAPTBL_DEPTH      2
#define CAPTBL_INTERNSZ   (sizeof(int*))
#define CAPTBL_INTERN_ORD 9 /* log(PAGE_SIZE/(2*(CAPTBL_DEPTH-1)*CAPTBL_INTERNSZ)) */
#define CAPTBL_LEAFSZ     (sizeof(struct cap_min))
#define CAPTBL_LEAF_ORD   7 /* log(PAGE_SIZE/(2*CAPTBL_LEAFSZ)) */

#ifdef CAP_FREE
#undef CAP_FREE
#endif

typedef enum {
	CAP_FREE = 0,
	CAP_SINV,		/* synchronous communication -- invoke */
	CAP_SRET,		/* synchronous communication -- return */
	CAP_ASND,		/* async communication; sender */
	CAP_ARCV,               /* async communication; receiver */
	CAP_THD,                /* thread */
	CAP_COMP,               /* component */
	CAP_CAPTBL,             /* capability table */
	CAP_PGTBL,              /* page-table */
	CAP_FRAME, 		/* untyped frame within a page-table */
	CAP_VM, 		/* mapped virtual memory within a page-table */
} cap_t;

typedef unsigned long capid_t;

/* 
 * The values in this enum are the order of the size of the
 * capabilities in this cacheline, offset by CAP_SZ_OFF (to compress
 * memory).
 */
typedef enum {
	CAP_SZ_16B = 0,
	CAP_SZ_32B = 1,
	CAP_SZ_64B = 2,
	CAP_SZ_ERR = 3,
} cap_sz_t;
/* the shift offset for the *_SZ_* values */
#define	CAP_SZ_OFF   4 
/* The allowed amap bits of each size */
#define	CAP_MASK_16B ((1<<4)-1)
#define	CAP_MASK_32B (1 | (1<<2))
#define	CAP_MASK_64B 1

/* a function instead of a struct to enable inlining + constant prop */
static inline cap_sz_t
__captbl_cap2sz(cap_t c)
{
	/* TODO: optimize for invocation and return */
	switch (c) {
	case CAP_CAPTBL: case CAP_THD:   
	case CAP_PGTBL:  case CAP_SRET: return CAP_SZ_16B;
	case CAP_SINV:   case CAP_COMP: return CAP_SZ_32B;
	case CAP_ASND:   case CAP_ARCV: return CAP_SZ_64B;
	default:                        return CAP_SZ_ERR;
	}
}
static inline unsigned long __captbl_cap2bytes(cap_t c)
{ return 1<<(__captbl_cap2sz(c)+CAP_SZ_OFF); }

typedef enum {
	CAP_FLAG_RO    = 1,
	CAP_FLAG_LOCAL = 1<<1,
	CAP_FLAG_RCU   = 1<<2,
} cap_flags_t;

#define CAP_HEAD_AMAP_SZ  4
#define CAP_HEAD_SZ_SZ    2
#define CAP_HEAD_FLAGS_SZ 3
#define CAP_HEAD_TYPE_SZ  7

/* 
 * This is the header for each capability.  Includes information about
 * allowed operations (read/write for specific cores), the size of the
 * capabilities in a cache-line, and the type of the capability.
 */
struct cap_header {
	u16_t       poly;
	/* 
	 * Size is only populated on cache-line-aligned entries.
	 * Applies to all caps in that cache-line 
	 */
	u8_t        amap  : CAP_HEAD_AMAP_SZ; 	/* allocation map */
	cap_sz_t    size  : CAP_HEAD_SZ_SZ; 	
	cap_flags_t flags : CAP_HEAD_FLAGS_SZ;
	cap_t       type  : CAP_HEAD_TYPE_SZ;
	u8_t        post[0];
} __attribute__((packed));

struct cap_min {
	struct cap_header h;
	char padding[(4*sizeof(int))-sizeof(struct cap_header)];
};

/* Capability structure to a capability table */
struct cap_captbl {
	struct cap_header h;
	struct captbl *captbl;
	u32_t lvl; 		/* what level are the captbl nodes at? */
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
__captbl_init(struct ert_intern *a, int leaf)
{ 
	if (leaf) return;
	a->next = NULL; 
}

static void
captbl_init(void *node, int leaf)
{
	int i;

	if (!leaf) {
		for (i = 0 ; i < 1<<CAPTBL_INTERN_ORD ; i++) {
			struct ert_intern *a;
			a = (struct ert_intern *)(((char *)node) + (i * CAPTBL_INTERNSZ));
			a->next = NULL; 
		}
	} else {
		for (i = 0 ; i < 1<<CAPTBL_LEAF_ORD ; i++) {
			struct cap_header *p = &(((struct cap_min*)node)[i].h);
			p->size  = CAP_SZ_64B;
			p->type  = CAP_FREE;
			p->amap  = 0;
			p->flags = 0;
		}
	}
}

static inline CFORCEINLINE void *
__captbl_getleaf(struct ert_intern *a, void *accum)
{
	(void)accum;
	unsigned long off, mask;
	struct cap_header *h = (struct cap_header *)CT_MSK(a, CACHELINE_ORDER);
	struct cap_header *c = (struct cap_header *)CT_MSK(a, h->size + CAP_SZ_OFF);
	/* 
	 * We could do error checking here to make sure that a == c,
	 * if we didn't want to avoid the extra branches:
	 * if (unlikely(a == (void*)c)) return NULL;
	 */

	/* 
	 * This requires explanation.  We want to avoid a conditional
	 * to check if this slot in the allocation map for the cache
	 * line is free or not.
	 */
	off  = (struct cap_min*)c - (struct cap_min*)h; /* ptr math */
	mask = (h->amap & (1<<off)) >> off;		/* 0 or 1, depending */
	mask--;						/* 0 or 0xFFFF... */
	c = (struct cap_header *)((unsigned long)c & ~mask); /* NULL, or the address */

	return c; 
}

static inline int __captbl_setleaf(struct ert_intern *a, void *v)
{ (void)a; (void)v; assert(0); return -1; }

#define CT_DEFINITVAL NULL
ERT_CREATE(__captbl, captbl, CAPTBL_DEPTH, CAPTBL_INTERN_ORD, CAPTBL_INTERNSZ,
	   CAPTBL_LEAF_ORD, CAPTBL_LEAFSZ, CT_DEFINITVAL, __captbl_init, ert_defget,
	   ert_defisnull, ert_defset, __captbl_allocfn, __captbl_setleaf,
	   __captbl_getleaf, ert_defresolve);

static struct captbl *captbl_alloc(void *page) { return __captbl_alloc(&page); }

static inline int
__captbl_header_validate(struct cap_header *h, cap_sz_t sz)
{ 
	cap_sz_t mask;
	/* compiler should optimize away the branches here */
	switch (sz) {
	case CAP_SZ_16B: mask = CAP_MASK_16B; break; 
	case CAP_SZ_32B: mask = CAP_MASK_32B; break; 
	case CAP_SZ_64B: mask = CAP_MASK_64B; break;
	default:         mask = 0;            break;
	}

	if (unlikely(sz != h->size)) return 1;
	return h->amap & ~mask;
}

static inline void *
captbl_lkup_lvl(struct captbl *t, capid_t cap, u32_t start_lvl, u32_t end_lvl)
{ 
	if (unlikely(cap >= __captbl_maxid())) return NULL;
	return __captbl_lkupani(t, cap, start_lvl, end_lvl, NULL); 
}

/* 
 * This function is the fast-path used for capability lookup in the
 * invocation path.
 */
static inline struct cap_header *captbl_lkup(struct captbl *t, capid_t cap)
{ return captbl_lkup_lvl(t, cap, 0, CAPTBL_DEPTH+1); }

static inline int
__captbl_store(unsigned long *addr, unsigned long new, unsigned long old)
{ if (*addr != old) return -1; *addr = new; return 0; }
#define CTSTORE(a, n, o) __captbl_store((unsigned long *)a, *(unsigned long *)n, *(unsigned long *)o)
#define cos_throw(label, errno) { ret = (errno); goto label; }

//#include <stdio.h>

static inline struct cap_header *
captbl_add(struct captbl *t, capid_t cap, cap_t type, int *retval)
{ 
	struct cap_header *p, *h;
	struct cap_header l, o;
	int ret = 0, off;
	cap_sz_t sz = __captbl_cap2sz(type);

	if (unlikely(sz == CAP_SZ_ERR)) cos_throw(err, -EINVAL);
	if (unlikely(cap >= __captbl_maxid())) cos_throw(err, -EINVAL);
	p = __captbl_lkupan(t, cap, CAPTBL_DEPTH, NULL); 
	if (unlikely(!p)) cos_throw(err, -EPERM);
	h = (struct cap_header *)CT_MSK(p, CACHELINE_ORDER);
	l = o = *h;
	if (unlikely(l.flags & CAP_FLAG_RO)) cos_throw(err, -EPERM);

	off = (struct cap_min*)p - (struct cap_min*)h; /* ptr math */
	assert(off >= 0 && off < CAP_HEAD_AMAP_SZ);
	/* already allocated? */
	if (unlikely(l.amap & (1<<off))) cos_throw(err, -EEXIST);
	if (unlikely((l.amap && (l.size != sz)) || 
		     l.size < sz)) cos_throw(err, -EEXIST);
	l.amap |= 1<<off;
	if (l.size != sz) {
		assert(l.size > sz);
		l.size = sz;
	}
	if (unlikely(__captbl_header_validate(&l, sz))) cos_throw(err, -EINVAL);

	/* FIXME: we should _not_ do this here.  This should be done
	 * in step 3 of the protocol for setting capabilities, not 1 */
	if (p == h) l.type = type;
	if (CTSTORE(h, &l, &o)) cos_throw(err, -EEXIST); /* commit */
	/* FIXME: same as above */
	if (p != h) p->type = type;
	
	assert(p == __captbl_lkupan(t, cap, CAPTBL_DEPTH+1, NULL));
	*retval = ret;
	return p;
err:
	*retval = ret;
	return NULL;
}

static inline int
captbl_del(struct captbl *t, capid_t cap, cap_t type)
{
	struct cap_header *p, *h;
	struct cap_header l, o;
	int ret = 0, off;

	if (unlikely(cap >= __captbl_maxid())) cos_throw(err, -EINVAL);
	p = __captbl_lkupan(t, cap, CAPTBL_DEPTH, NULL); 
	if (unlikely(!p)) cos_throw(err, -EPERM);
	if (p != __captbl_getleaf((void*)p, NULL)) cos_throw(err, -EINVAL);
	if (p->type == type) cos_throw(err, -EINVAL);

	h   = (struct cap_header *)CT_MSK(p, CACHELINE_ORDER);
	off = (struct cap_min*)p - (struct cap_min*)h;
	assert(off >= 0 && off < CAP_HEAD_AMAP_SZ);
	l = o = *h;

	/* Do we want RO to prevent deletions? */
	if (unlikely(l.flags & CAP_FLAG_RO)) cos_throw(err, -EPERM);
	if (unlikely(!(l.amap & (1<<off)))) cos_throw(err, -ENOENT);

	if (h == p) l.type  = CAP_FREE;
	else        p->type = CAP_FREE;
	/* FIXME: store barrier on non-x86 */
	/* new map, removing the current allocation */
	l.amap &= (~(1<<off)) & ((1<<CAP_HEAD_AMAP_SZ)-1);
	if (l.amap == 0) l.size = CAP_SZ_64B; /* no active allocations... */
	if (CTSTORE(h, &l, &o)) cos_throw(err, -EEXIST); /* commit */
err:
	return ret;
}

static inline u32_t captbl_maxdepth(void) { return __captbl_maxdepth(); }

/* 
 * Extend a capability table up to a depth using the memory passed in
 * via memctxt.  Returns a negative value on error, a positive value
 * if the table is extended, but the depth specified was too deep
 * (requiring more memory), and zero on unqualified success.
 */
static inline int
captbl_expand(struct captbl *t, capid_t cap, u32_t depth, void *memctxt)
{
	int ret;

	if (unlikely(cap > __captbl_maxid() ||
		     depth > captbl_maxdepth())) return -EINVAL;
	ret = __captbl_expandn(t, cap, depth, NULL, &memctxt, NULL);
	if (unlikely(memctxt)) return -EEXIST;
	if (unlikely(ret))     return 1; /* extended successfully, but incorrect depth value */

	return 0;
}

/* 
 * Prune off part of the capability tree at the specified depth along
 * the path to the specified capability number.  depth == 1 will prune
 * off all but the root.
 */
static void *
captbl_prune(struct captbl *t, capid_t cap, u32_t depth, int *retval)
{
	void **intern, *p, *new;
	int ret = 0;

	if (unlikely(cap   >= __captbl_maxid() || 
		     depth >= captbl_maxdepth())) cos_throw(err, -EINVAL);
	intern = __captbl_lkupan(t, cap, depth, NULL); 
	if (unlikely(!intern)) cos_throw(err, -EPERM);
	p = *intern;
	new = CT_DEFINITVAL;
	if (CTSTORE(intern, &new, &p)) cos_throw(err, -EEXIST); /* commit */
done:
	*retval = ret;
	return p;
err:
	p = NULL;
	goto done;
}

static struct captbl *
captbl_create(void *page)
{
	struct captbl *ct;
	int ret;

	assert(page);
	ct = captbl_alloc(page);
	assert(ct);
	/* 
	 * replace hard-coded sizes with calculations based on captbl
	 * depth, and intern and leaf sizes/orders
	 */
	captbl_init(&((char*)page)[PAGE_SIZE/2], 1);
	ret = captbl_expand(ct, 0, captbl_maxdepth(), &((char*)page)[PAGE_SIZE/2]);
	assert(!ret);

	return ct;
}

int captbl_activate(struct captbl *t, capid_t cap, capid_t capin, struct captbl *toadd, u32_t lvl);
int captbl_deactivate(struct captbl *t, capid_t cap, capid_t  capin);
int captbl_activate_boot(struct captbl *t, unsigned long cap);
static void cap_init(void) {}

#endif /* CAPTBL_H */
