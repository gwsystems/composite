/***
 * Copyright 2012 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#ifndef CVECT_COMPRESSED_H
#define CVECT_COMPRESSED_H

/***
 * Vector mapping an id (30 bit u32_t) to a word-size value (e.g. an
 * address).  Does path compression (decreasing the depth of the tree
 * by ignoring shared bits between entries in sub-trees) and level
 * compression (using variable sized index nodes in each subtree to
 * use larger allocations when ids are more dense).  These, together,
 * hopefully result in a data-structure that is quite fast (though not
 * as fast as cvect.h), and has reasonable memory usage
 * characteristics (unlike cvect.h for sparse id distributions).
 *
 * This is the first draft.  It does _not_ currently provide level
 * compression.
 *
 * Public functions are those without "__" prepended onto them.
 */

#include <string.h> 

#ifdef LINUX_TEST
 #ifndef CVECTC_ALLOC
  #define CVECTC_ALLOC(sz)   malloc(sz)
  #define CVECTC_FREE(x, sz) free(x)
 #endif
 typedef unsigned int u32_t;
 #define likely(x) (x)
 #define unlikely(x) (x)
#else
 #include <cos_component.h>
 #ifndef CVECTC_ALLOC
  #error "Please pound define CVECTC_ALLOC and CVECTC_FREE"
 #endif
#endif

#define CVECTC_INIT_VAL    0
#define CVECTC_MIN_ENTRIES 8
#define CVECTC_MIN_ORDER   3
#define CVECTC_WORD_SZ     32
#define CVECTC_MAX_ID_SZ   30
#define CVECTC_MAX_DEPTH   ((CVECTC_WORD_SZ+CVECTC_MIN_ORDER-1)/CVECTC_MIN_ORDER) /* complicated to round up... */

struct cvcdir {
	u32_t leaf:1, size:6, ignore:6, nentries:19; /* nentries ignored */
	struct cvcentry *next;
};

struct cvcleaf {
	u32_t leaf:1, id:31;
	void *val;
};

struct cvcentry {
	union {
		/* .leaf = 1 */
		struct cvcdir  d;
		/* .leaf = 0 */
		struct cvcleaf l;
	} e;
} __attribute__((packed));

static inline int __cvc_isleaf(struct cvcentry *e) { return !e->e.d.leaf; }
static inline struct cvcleaf *__cvc_leaf(struct cvcentry *e) 
{ assert(__cvc_isleaf(e));  return &e->e.l; }
static inline struct cvcdir *__cvc_dir(struct cvcentry *e) 
{ assert(!__cvc_isleaf(e)); return &e->e.d; }

struct cvectc {
	struct cvcentry d;
};

static inline void
__cvectc_dir_init(struct cvcentry *d, int ignored_sz, int next_sz, struct cvcentry *n)
{
	assert(d && n && next_sz <= CVECTC_WORD_SZ && ignored_sz <= CVECTC_WORD_SZ);
	d->e.d.next   = n;
	d->e.d.leaf   = 1;
	d->e.d.ignore = ignored_sz;
	d->e.d.size   = CVECTC_WORD_SZ-next_sz;
}

static inline void
__cvectc_leaf_init(struct cvcentry *l, u32_t id, void *value)
{
	assert(l);
	l->e.l.id   = id;
	l->e.l.val  = value;
	l->e.l.leaf = 0;
}

static inline void
cvectc_init(struct cvectc *v)
{ __cvectc_leaf_init((struct cvcentry *)&v->d, 0, CVECTC_INIT_VAL); }

static inline struct cvcentry *
__cvectc_next_lvl(struct cvcdir *d, u32_t id) 
{ return &d->next[(id << d->ignore) >> d->size]; }

static inline struct cvcleaf *
__cvectc_lookup_leaf(struct cvcentry *e, u32_t id)
{
	assert(e);
	while (!__cvc_isleaf(e)) e = __cvectc_next_lvl(__cvc_dir(e), id);
	return __cvc_leaf(e);
}

/* return not only the leaf, but the leaf's parent as well */
static inline struct cvcleaf *
__cvectc_lookup_leaf_prev(struct cvcentry *e, u32_t id, struct cvcdir **d)
{
	*d = NULL;
	assert(e);
	while (!__cvc_isleaf(e)) {
		*d = __cvc_dir(e);
		e  = __cvectc_next_lvl(__cvc_dir(e), id);
	}
	return __cvc_leaf(e);
}

/* Everything is optimized around the performance of this function... */
static inline void *
cvectc_lookup(struct cvectc *v, u32_t id)
{
	struct cvcleaf *l;
	
	l = __cvectc_lookup_leaf(&v->d, id);
	assert(l);
	if (likely(l->id == id)) return l->val;
	else                     return CVECTC_INIT_VAL;
}

static inline u32_t
__cvectc_prefix(u32_t v1, int sz)
{ return (v1 >> (CVECTC_WORD_SZ - sz)) << (CVECTC_WORD_SZ - sz); }

static inline int
__cvectc_prefix_match(u32_t v1, u32_t v2, int prefix_order)
{ return __cvectc_prefix(v1, prefix_order) == __cvectc_prefix(v2, prefix_order); }

static inline int
__cvectc_prefix_sz(u32_t v1, u32_t v2)
{
	int i, cnt = 0;
	u32_t out_of_bounds = ((u32_t)(~0)>>CVECTC_MAX_ID_SZ<<CVECTC_MAX_ID_SZ);
	assert((out_of_bounds & (v1 | v2)) == 0);
	for (i = CVECTC_MAX_ID_SZ-1 ; i > 0 && ((v1 >> i) == (v2 >> i)) ; i--, cnt++) ;
	return cnt;
}

static inline int
__cvectc_alloc_init(struct cvcentry *d, u32_t prefix, int prefix_sz, 
		    u32_t id, void *val, u32_t trie_id)
{
	struct cvcentry *n, saved, *l, *p;
	struct cvcdir *new_d;
	int i;

	memcpy(&saved, d, sizeof(struct cvcentry));
	n = CVECTC_ALLOC(sizeof(struct cvcentry) * CVECTC_MIN_ENTRIES);
	if (!n) return -1;

	for (i = 0 ; i < CVECTC_MIN_ENTRIES ; i++) {
		__cvectc_leaf_init(&n[i], prefix, CVECTC_INIT_VAL);
	}
	__cvectc_dir_init((struct cvcentry*)d, prefix_sz, CVECTC_MIN_ORDER, n);
	new_d = (struct cvcdir *)d;

	printf("prefix %x, sz %d\n", prefix, prefix_sz);
	/* initialize the previous, existing entry in the structure */
	p = __cvectc_next_lvl(new_d, trie_id);
	memcpy(p, &saved, sizeof(struct cvcentry));
	/* initialize the new entry in the structure */
	l = __cvectc_next_lvl(new_d, id);
	__cvectc_leaf_init(l, id, val);
	assert(p != l);

	return 0;
}

/* 
 * We need to add an id that does _not_ share the same prefix as
 * existed in the previous trie.  The ignore bits ignore too many
 * bits!  We need to expand out a level to accommodate for the
 * non-shared prefix, thus path _de_compress.
 */
static inline int
__cvectc_path_decompress(struct cvcentry *e, u32_t id, u32_t trie_id, void *val)
{
	int prefix_sz, nprefix_sz;
	u32_t nprefix;

	prefix_sz  = __cvc_isleaf(e) ? CVECTC_MAX_ID_SZ : __cvc_dir(e)->ignore;
	nprefix_sz = (__cvectc_prefix_sz(id, trie_id) >> CVECTC_MIN_ORDER) << CVECTC_MIN_ORDER;
	nprefix    = __cvectc_prefix(id, prefix_sz);
	printf("id %x, old_id %x, prefixsz %d, nprefixsz %d, prefix %x\n", 
	       id, trie_id, prefix_sz, nprefix_sz, nprefix);

	if (!__cvc_isleaf(e)) assert(nprefix_sz <= __cvc_dir(e)->ignore);
	assert(nprefix_sz <= nprefix_sz);
	return __cvectc_alloc_init(e, nprefix, prefix_sz, id, val, trie_id);
}

static int
cvectc_add(struct cvectc *v, void *val, u32_t id)
{
	struct cvcleaf *l;
	struct cvcdir *p;
	struct cvcentry *e;

	assert(v && val != CVECTC_INIT_VAL);
	assert(CVECTC_INIT_VAL == cvectc_lookup(v, id));
	/* 2 bits ignored: one bit required for leaf in cvcleaf */
	assert(id <= (u32_t)(~0)>>2);

	l = __cvectc_lookup_leaf_prev(&v->d, id, &p);
	assert(l);
	printf("p %p, l %p\n", p, l);
	/* 
	 * Conditions:
	 * !p                 -> leaf l is root of tree
	 * l->val == INIT_VAL -> empty spot, but the prefix might not match
	 * !prefix_match(...) -> decompression required somewhere
	 */
	/* id already present! */
	if (unlikely(l->id == id)) return -1;
	/* empty entry, or this id fits its prefix */
	printf("y\n");
	if (!p || __cvectc_prefix_match(id, l->id, p->ignore)) {
		printf("z\n");
		/* initial entry */
		if (l->val == CVECTC_INIT_VAL) {
			__cvectc_leaf_init((struct cvcentry *)l, id, val);
		}
		/* entry exists, and we need to expand the tree... */
		else {
			if (__cvectc_path_decompress((struct cvcentry *)l, id, l->id, val)) return -1;
		}
		assert(cvectc_lookup(v, id) == val);
		return 0;
	}
	/* else: no prefix match */

	printf("a\n");
	/* ...otherwise do a long-form lookup through the structure of the tree */
	e = &v->d;
	assert(!__cvc_isleaf(e));
	do {
		struct cvcdir *d = __cvc_dir(e);

		printf("b\n");

		if (!__cvectc_prefix_match(id, l->id, d->ignore)) {
			printf("c\n");
			if (__cvectc_path_decompress((struct cvcentry *)d, id, l->id, val)) return -1;
			break;
		}
		e = __cvectc_next_lvl(d, id);
	} while (!__cvc_isleaf(e));
	assert(cvectc_lookup(v, id) == val);

	return 0;
}

static inline int __cvectc_size(struct cvcdir *d) 
{ return 1 << (CVECTC_WORD_SZ - d->size - 1); }

static inline int
__cvectc_nentries(struct cvcdir *d, int sz, struct cvcentry **entry)
{
	int i, cnt = 0;
	struct cvcentry *e;

	assert(d && !__cvc_isleaf((struct cvcentry *)d));
	assert(sz >= CVECTC_MIN_ENTRIES); /* check entries vs. order */
	e = d->next;
	for (i = 0 ; i < sz ; i++) {
		if (!__cvc_isleaf(&e[i]) || 
		    __cvc_leaf(&e[i])->val != CVECTC_INIT_VAL) {
			*entry = &e[i];
			cnt++;
		}
	}

	return cnt;
}

/* 
 * How entries does this directory's children have, out of how many
 * entries?  If there are a significant amount of populated entries
 * compared to the total, then we should probably do level compression
 * here.
 */
static inline int 
__cvectc_nentries_children(struct cvcdir *d, int sz, int *tot_sz)
{
	int i, cnt = 0;
	struct cvcentry *e;

	assert(d && !__cvc_isleaf((struct cvcentry *)d));
	assert(sz >= CVECTC_MIN_ENTRIES); /* check entries vs. order */
	assert(tot_sz);
	*tot_sz = 0;
	e       = d->next;
	for (i = 0 ; i < sz ; i++) {
		if (!__cvc_isleaf(&e[i])) {
			struct cvcentry *e;
			struct cvcdir *d = __cvc_dir(&e[i]);

			*tot_sz += __cvectc_size(d);
			cnt     += __cvectc_nentries(d, __cvectc_size(d), &e);
		}
	}
	
	return cnt;
}

/* 
 * More prefix is shared than is taken into account in the "ignore"
 * bits for this level due to an excess of levels.  Remove those
 * excess levels, and add their shared prefix to the ignore bits for
 * this level.
 */
static inline void
__cvectc_path_compress(struct cvcdir *d, u32_t id)
{
	struct cvcentry *c;
	int sz = __cvectc_size(d);
	struct cvcentry *p;

	assert(d && !__cvc_isleaf((struct cvcentry *)d) && 
	       __cvectc_nentries(d, sz, &p) == 0);
	
	c = d->next;
	__cvectc_leaf_init((struct cvcentry *)d, id, CVECTC_INIT_VAL);
	CVECTC_FREE(c, sz);
}

static void
cvectc_rem(struct cvectc *v, u32_t id)
{
	struct cvcentry *e, *s;	 /* entry and saved entry */
	struct cvcdir *p;        /* previous level */
	struct cvcleaf *l;
	int nent, size;

	e = &v->d;
	l = __cvectc_lookup_leaf_prev(e, id, &p);
	assert(l);
	/* remove the item, but maintain the prefix */
	__cvectc_leaf_init((struct cvcentry *)l, id, CVECTC_INIT_VAL);
	/* No previous level?  Nothing to compress: we're done. */
	if (!p) return;

	size = __cvectc_size(p);
	nent = __cvectc_nentries(p, size, &s);
	/* No entries in the subtree: kill it */
	if (nent == 0) __cvectc_path_compress(p, id);
	/* 
	 * One entry in the subtree: kill the subtree, and make the
	 * reference to the subtree, reference to the entry instead.
	 */
	else if (nent == 1) {
		struct cvcentry saved, *t;

		assert(__cvc_isleaf(s) || 
		       __cvectc_nentries(__cvc_dir(s), size, &t) > 1);
		/* save the active entry */
		memcpy(&saved, s, sizeof(struct cvcentry));
		__cvectc_leaf_init(s, id, CVECTC_INIT_VAL);
		/* get rid of the subtree */
		__cvectc_path_compress(p, id);
		/* put the saved entry in its place */
		memcpy(__cvectc_lookup_leaf(&v->d, id), &saved, sizeof(struct cvcentry));
	}

	return;
}

#endif
