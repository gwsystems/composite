/***
 * Copyright 2012 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors:
 * Gabriel Parmer, gparmer@gwu.edu,
 * - Jan, 2012 -- n-ary tree
 * - Aug, 2012 -- initial versions of path and level compression
 */

#ifndef CVECT_COMPRESSED_H
#define CVECT_COMPRESSED_H

/***
 * This is a data-structure behaving like a vector mapping an id (30
 * bit u32_t) to a word-size value (e.g. an address).  Is a PLC tree:
 * one that does Path Compression (decreasing the depth of the tree by
 * ignoring shared bits between entries in sub-trees) and Level
 * Compression (using variable sized index nodes in each subtree to
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
#define CVECTC_ALLOC(sz) malloc(sz)
/*
 * Note that free is non-standard here, taking the size as an
 * argument.  This might simplify intelligent reallocing using a
 * buddy allocator.
 */
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

#define CVECTC_INIT_VAL 0
#define CVECTC_MIN_ORDER 1
#define CVECTC_MIN_ENTRIES (1 << CVECTC_MIN_ORDER)
#define CVECTC_MAX_SZ 32
#define CVECTC_MAX_ID_SZ 30
#define CVECTC_MAX_DEPTH (CVECTC_MAX_ID_SZ / CVECTC_MIN_ORDER)

/* parameter validity checking: */
#if (CVECTC_MIN_ORDER * (CVECTC_MAX_DEPTH)) != CVECTC_MAX_ID_SZ
#error "cvectc: The max id size must be a multiple of the min order of internal nodes"
#endif
#if (CVECTC_MIN_ORDER < 1)
#error "cvectc: the minimum node order is less than 1."
#endif

/*
 * Invariants:  2^ignore <= CVECTC_MAX_SZ.   nsubdir <= 2^__cvectc_size(d)
 */
struct cvcdir {
	u32_t            leaf : 1, ignore : 7, size : 6, nsubdir : 18;
	struct cvcentry *next;
};

struct cvcleaf {
	u32_t leaf : 1, present : 1, id : 30;
	void *val;
};

struct cvcentry {
	union {
		/* .leaf = 1 */
		struct cvcdir d;
		/* .leaf = 0 */
		struct cvcleaf l;
	} e;
} __attribute__((packed));

struct cvectc {
	struct cvcentry d;
};

#ifdef CVECTC_STATS
#define SIZE_REC 8
static struct cvectc_stats {
	int nodes, mem, nentries;
	struct lvlsz {
		int sz, cnt, tot;
	} lvlsizes[SIZE_REC];
} __cvectc_stats = {.lvlsizes = {
                      {.sz = 2},
                      {.sz = 4},
                      {.sz = 8},
                      {.sz = 16},
                      {.sz = 32},
                      {.sz = 64},
                      {.sz = 128},
                      {.sz = 0}, /* catch all */
                    }};

static void
cvectc_stats_node(int sz)
{
	int i;

	if (sz > 0)
		__cvectc_stats.nodes++;
	else
		__cvectc_stats.nodes--;
	__cvectc_stats.mem += sz * sizeof(struct cvcentry);

	for (i = 0; i < SIZE_REC - 1; i++) {
		if (__cvectc_stats.lvlsizes[i].sz == sz) {
			__cvectc_stats.lvlsizes[i].tot++;
			__cvectc_stats.lvlsizes[i].cnt++;
			return;
		} else if (__cvectc_stats.lvlsizes[i].sz == -1 * sz) {
			__cvectc_stats.lvlsizes[i].cnt--;
			return;
		}
	}
	if (sz > 0) {
		__cvectc_stats.lvlsizes[SIZE_REC - 1].tot++;
		__cvectc_stats.lvlsizes[SIZE_REC - 1].cnt++;
		return;
	} else {
		__cvectc_stats.lvlsizes[SIZE_REC - 1].cnt--;
		return;
	}
}
static void
cvectc_stats_nent(int delta)
{
	__cvectc_stats.nentries += delta;
}
static void
cvectc_stats(void)
{
	int nent = __cvectc_stats.nentries, i;
	printf("Tree nodes %d, memory %d, entries %d, %d memory/entry\n", __cvectc_stats.nodes, __cvectc_stats.mem,
	       nent, __cvectc_stats.mem / (nent ? nent : 1));
	for (i = 0; i < SIZE_REC; i++) {
		if (__cvectc_stats.lvlsizes[i].cnt == 0) continue;
		printf("\tnode %d: %d, total %d\n", __cvectc_stats.lvlsizes[i].sz, __cvectc_stats.lvlsizes[i].cnt,
		       __cvectc_stats.lvlsizes[i].tot);
	}
}
#else
#define cvectc_stats_node(v, d)
#define cvectc_stats_nent(v, d)
#define cvectc_stats()
#endif

static inline int
__cvc_isleaf(struct cvcentry *e)
{
	return !e->e.d.leaf;
}
static inline struct cvcleaf *
__cvc_leaf(struct cvcentry *e)
{
	assert(__cvc_isleaf(e));
	return &e->e.l;
}
static inline struct cvcdir *
__cvc_dir(struct cvcentry *e)
{
	assert(!__cvc_isleaf(e));
	return &e->e.d;
}
static inline int
__cvc_ispresent(struct cvcleaf *l)
{
	assert(__cvc_isleaf((struct cvcentry *)l));
	return l->present;
}
static inline int
__cvectc_size_order(struct cvcdir *d)
{
	return CVECTC_MAX_SZ - d->size;
}
static inline int
__cvectc_size(struct cvcdir *d)
{
	return 1 << __cvectc_size_order(d);
}

static inline void
__cvectc_dir_init(struct cvcentry *d, int ignored_sz, int next_sz, struct cvcentry *n)
{
	assert(d && n && next_sz <= CVECTC_MAX_SZ && ignored_sz <= CVECTC_MAX_SZ);
	d->e.d.next    = n;
	d->e.d.leaf    = 1;
	d->e.d.ignore  = ignored_sz;
	d->e.d.size    = CVECTC_MAX_SZ - next_sz;
	d->e.d.nsubdir = 0;
}

/*
 * Thresholds for level compression: When to compress a level, and
 * when to decompress it.
 */
static inline int
__cvectc_upper_thresh(int sz, int amnt)
{
	return amnt >= (sz / 2 + sz / 4);
} /* 75% */
static inline int
__cvectc_lower_thresh(int sz)
{
	return sz / 4;
} /* 25% */

static inline void
__cvect_dir_inc_cnt(struct cvcentry *d)
{
	assert(!__cvc_isleaf(d));
	d->e.d.nsubdir++;
}

static inline void
__cvectc_leaf_init(struct cvcentry *l, u32_t id, void *value)
{
	assert(l);
	l->e.l.id      = id;
	l->e.l.val     = value;
	l->e.l.leaf    = 0;
	l->e.l.present = 1;
}

static inline void
__cvectc_leaf_init_empty(struct cvcentry *l, u32_t id)
{
	assert(l);
	l->e.l.id      = id;
	l->e.l.val     = CVECTC_INIT_VAL;
	l->e.l.leaf    = 0;
	l->e.l.present = 0;
}

static inline int
cvectc_init(struct cvectc *v)
{
	struct cvcentry *n;
	int              i;

	n = CVECTC_ALLOC(sizeof(struct cvcentry) * CVECTC_MIN_ENTRIES);
	if (!n) return -1;
	cvectc_stats_node(CVECTC_MIN_ENTRIES);

	for (i = 0; i < CVECTC_MIN_ENTRIES; i++) {
		__cvectc_leaf_init_empty(&n[i], 0);
	}
	__cvectc_dir_init(&v->d, CVECTC_MAX_SZ - CVECTC_MIN_ORDER, CVECTC_MIN_ORDER, n);

	return 0;
}

static inline struct cvcentry *
__cvectc_next_lvl(struct cvcdir *d, u32_t id)
{
	return &d->next[(id << d->ignore) >> d->size];
}

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
	/*
	 * If leaf is present then, val is valid, otherwise it should
	 * be CVECTC_INIT_VAL, which is what we'd want to return.
	 */
	if (likely(l->id == id))
		return l->val;
	else
		return CVECTC_INIT_VAL;
}

static inline u32_t
__cvectc_prefix(u32_t v1, int sz)
{
	return (v1 >> (CVECTC_MAX_SZ - sz)) << (CVECTC_MAX_SZ - sz);
}

static inline int
__cvectc_prefix_match(u32_t v1, u32_t v2, int prefix_order)
{
	return __cvectc_prefix(v1, prefix_order) == __cvectc_prefix(v2, prefix_order);
}

/* TODO: replace with WORD_SZ - log(v1 xor v2) */
static inline int
__cvectc_prefix_sz(u32_t v1, u32_t v2)
{
	int   i, cnt = 0;
	u32_t out_of_bounds = (((u32_t)(~0) >> CVECTC_MAX_ID_SZ) << CVECTC_MAX_ID_SZ);
	assert((out_of_bounds & (v1 | v2)) == 0);

	for (i = CVECTC_MAX_SZ - 1; i > 0 && ((v1 >> i) == (v2 >> i)); i--, cnt++)
		;

	return cnt;
}

static void
cvcprint(struct cvcentry *e)
{
	if (__cvc_isleaf(e)) {
		struct cvcleaf *l = __cvc_leaf(e);
		if (__cvc_ispresent(l))
			printf("[L: id %3d]\n", l->id);
		else
			printf("[L: <empty> id %3d]\n", l->id);
	} else {
		struct cvcdir *d = __cvc_dir(e);
		printf("[D: ignore %3d, sz %4d]\n", d->ignore, __cvectc_size(d));
	}
}

int cvectc_debug = 0;

static inline int
__cvectc_level_compress(struct cvcdir *p, u32_t id)
{
	int              i, sz, nsz, subsz = 0, uniform_subsz = 1, pignored;
	struct cvcentry *e, *n, *t;
	struct cvcleaf * l;

	if (cvectc_debug) cvcprint((struct cvcentry *)p);
	l = __cvectc_lookup_leaf((struct cvcentry *)p, id);
	assert(__cvc_ispresent(l));
	assert(l->id == id);

	//	printf(">>> level compression when inserting %d\n", id);

	e  = p->next;
	sz = __cvectc_size(p);
	/*
	 * Get the sizes of the subdirectories.  We want to know if we
	 * can actually do the compression, or if some subdirectories
	 * are too large. TODO: add logic to shrink sublevels that are
	 * too large to be brought into the new, larger directory.
	 */
	for (i = 0; i < sz; i++) {
		struct cvcentry *iter = &e[i];
		struct cvcdir *  d;
		int              __sz;

		if (__cvc_isleaf(iter)) continue;
		d    = __cvc_dir(iter);
		__sz = __cvectc_size(d);
		/* are all of the subsizes the same? */
		if (!subsz) subsz = __sz;
		if (__sz != subsz) {
			uniform_subsz = 0;
			break;
		}
	}
	assert(subsz);

	/*
	 * TODO: add logic for demotion of sublevels to enable any
	 * non-uniformly sized subdirectories to be
	 * compressed/decompressed.
	 */
	if (!uniform_subsz) return 0;

	nsz = subsz * CVECTC_MIN_ENTRIES;
	n   = CVECTC_ALLOC(sizeof(struct cvcentry) * nsz);
	if (!n) return -1;
	cvectc_stats_node(nsz);
	for (i = 0; i < nsz; i++) __cvectc_leaf_init_empty(&n[i], id);

	/* t = __cvectc_next_lvl(p, id); */
	/* t = __cvectc_next_lvl(__cvc_dir(t), id); */
	/* printf("*** Pre\n"); */
	/* cvcprint((struct cvcentry *)p); */
	/* printf("->\n"); */
	/* cvcprint(t); */
	/* printf("L1: id %x, ignored %x, size %x\n",  */
	/*        id, id << p->ignore, (id << p->ignore) >> p->size); */
	/* t = __cvectc_next_lvl(p, id); */
	/* printf("L2: id %x, ignored %x, size %x\n",  */
	/*        id, id << __cvc_dir(t)->ignore, (id << __cvc_dir(t)->ignore) >> __cvc_dir(t)->size); */
	/* printf("***\n"); */

	/* setup the new, larger parent node */
	pignored = p->ignore;
	__cvectc_dir_init((struct cvcentry *)p, pignored, __cvectc_size_order(p) + CVECTC_MIN_ORDER, n);
	assert(__cvectc_size(p) == nsz);
	assert(p->ignore == pignored);
	assert(p->next == n);

	/*
	 * For each entry in the old directory, populate the new,
	 * larger directory.  For leaves, we simply copy the leaf up.
	 * For directories, we need to copy all directory entries into
	 * the new parent.
	 */
	for (i = 0; i < sz; i++) {
		struct cvcentry *sub, *target, *ndir_off;
		struct cvcdir *  d;
		int              dir_off;

		sub = &e[i];
		/* leaves */
		if (__cvc_isleaf(sub)) {
			struct cvcleaf *l = __cvc_leaf(sub);

			target = __cvectc_next_lvl(p, l->id);
			__cvectc_leaf_init(target, l->id, l->val);
			/* printf("copying leaf %d into %p.\n", l->id, target); */
			continue;
		}

		/* directories */
		d = __cvc_dir(sub);
		assert(__cvectc_size(d) == subsz);
		/* printf("processing directory with ignore %d, and size %d\n", */
		/*        d->ignore, __cvectc_size(d)); */

		/* offset into new compressed level */
		dir_off  = i * subsz;
		ndir_off = &n[dir_off];

		/*
		 * Subtle case here: If this node cannot properly
		 * separately index the children of this directory,
		 * then we have to keep the subtree.
		 */
		if (d->ignore > p->ignore + subsz) {
			memcpy(ndir_off, d, sizeof(struct cvcentry));
		}
		/*
		 * If we can discriminate between the children of the
		 * subtree given our ignore bits and size, then we can
		 * copy those children into this new, larger directory
		 * directly.
		 */
		else {
			/* bounds checking */
			assert(ndir_off >= n && (&ndir_off[subsz] <= &n[nsz]));
			memcpy(ndir_off, d->next, subsz * sizeof(struct cvcentry));
			CVECTC_FREE(d->next, subsz);
			cvectc_stats_node(-1 * subsz);
		}
	}

	if (cvectc_debug) cvcprint((struct cvcentry *)p);
	/* t = __cvectc_next_lvl(p, id); */

	/* for (i = 0 ; i < sz ; i++) { */
	/* 	int j; */
	/* 	struct cvcentry *ent = &e[i]; */
	/* 	struct cvcdir *dir; */

	/* 	if (__cvc_isleaf(ent)) { */
	/* 		cvcprint(ent); */
	/* 		continue; */
	/* 	} */
	/* 	dir = __cvc_dir(ent); */

	/* 	for (j = 0 ; j < subsz ; j++) { */
	/* 		struct cvcentry *sube = &((dir->next)[j]); */
	/* 		cvcprint(sube); */
	/* 	} */
	/* } */

	/* printf("*** Post\n"); */
	/* cvcprint((struct cvcentry *)p); */
	/* printf("->\n"); */
	/* cvcprint(t); */
	/* printf("L1: id %x, ignored %x, size %x\n",  */
	/*        id, id << p->ignore, (id << p->ignore) >> p->size); */
	/* printf("***\n"); */

	/* for (i = 0 ; i < nsz ; i++) { */
	/* 	cvcprint(&n[i]); */
	/* } */

	l = __cvectc_lookup_leaf((struct cvcentry *)p, id);
	assert(__cvc_ispresent(l));
	assert(l->id == id);

	CVECTC_FREE(e, sz);
	cvectc_stats_node(-1 * sz);

	/* printf("\n"); */
	return 1;
}

/*
 * Allocate a new tree node, and link it in at a location.
 */
static inline int
__cvectc_alloc_link(struct cvcentry *e, u32_t nprefix, int prefix_sz)
{
	struct cvcentry *n;
	int              i;

	n = CVECTC_ALLOC(sizeof(struct cvcentry) * CVECTC_MIN_ENTRIES);
	if (!n) return -1;
	cvectc_stats_node(CVECTC_MIN_ENTRIES);

	for (i = 0; i < CVECTC_MIN_ENTRIES; i++) {
		__cvectc_leaf_init_empty(&n[i], nprefix);
	}
	__cvectc_dir_init(e, prefix_sz, CVECTC_MIN_ORDER, n);

	return 0;
}

/*
 * Find the prefix information associated with a pair of ids.  This
 * will be used to populate the directory "ignore" information, so has
 * to abide by tree node sizing constraints (i.e. aligned on a
 * CVECTC_MIN_ORDER boundary).
 */
static inline void
__cvectc_prefix_info(struct cvcentry *e, u32_t id, u32_t trie_id, u32_t *__prefix, int *__prefix_sz)
{
	int   prefix_sz, nprefix_sz;
	u32_t nprefix;

	prefix_sz  = __cvc_isleaf(e) ? CVECTC_MAX_SZ - CVECTC_MIN_ORDER : __cvc_dir(e)->ignore;
	nprefix_sz = __cvectc_prefix_sz(id, trie_id);
	/* if CVECTC_MIN_ORDER is a power of two, this is efficient */
	nprefix_sz = nprefix_sz - (nprefix_sz % CVECTC_MIN_ORDER);
	nprefix    = __cvectc_prefix(id, nprefix_sz);

	if (!__cvc_isleaf(e)) assert(nprefix_sz <= __cvc_dir(e)->ignore);
	assert(nprefix_sz <= prefix_sz);

	*__prefix_sz = nprefix_sz;
	*__prefix    = nprefix;
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
	int             nprefix_sz;
	u32_t           nprefix;
	struct cvcentry saved, *l, *p;
	struct cvcdir * new_d;

	__cvectc_prefix_info(e, id, trie_id, &nprefix, &nprefix_sz);

	memcpy(&saved, e, sizeof(struct cvcentry));
	if (__cvectc_alloc_link(e, nprefix, nprefix_sz)) goto err;
	new_d = __cvc_dir(e);

	/* initialize the previous, existing entry in the structure */
	p = __cvectc_next_lvl(new_d, trie_id);
	memcpy(p, &saved, sizeof(struct cvcentry));
	/*
	 * If we are adding to the directory a directory, increment
	 * that count
	 */
	if (!__cvc_isleaf(p)) __cvect_dir_inc_cnt((struct cvcentry *)new_d);

	/* initialize the new entry in the structure */
	l = __cvectc_next_lvl(new_d, id);
	__cvectc_leaf_init(l, id, val);

	assert(p != l);
	return 0;
err:
	return -1;
}

static inline int __cvectc_nsubdirs(struct cvcdir *d);

static int
cvectc_add(struct cvectc *v, void *val, u32_t id)
{
	struct cvcleaf * l;
	struct cvcdir *  p;
	struct cvcentry *e;

	assert(v && val != CVECTC_INIT_VAL);
	/* 2 bits ignored: one bit required for leaf in cvcleaf */
	assert(id <= (u32_t)(~0) >> (CVECTC_MAX_SZ - CVECTC_MAX_ID_SZ));

	l = __cvectc_lookup_leaf_prev(&v->d, id, &p);
	assert(l && p);
	/*
	 * Conditions:
	 * !p                 -> leaf l is root of tree
	 * l->val == INIT_VAL -> empty spot, but the prefix might not match
	 * !prefix_match(...) -> decompression required somewhere
	 */
	/* id already present! */
	if (unlikely(l->id == id && __cvc_ispresent(l))) return -1;

	/* empty entry, or this id fits its prefix */
	if (__cvectc_prefix_match(id, l->id, p->ignore)) {
		/* initial entry...populate it! */
		if (!__cvc_ispresent(l)) {
			__cvectc_leaf_init((struct cvcentry *)l, id, val);
		}
		/*
		 * Entry exists so there's a conflict, and we need to
		 * expand the tree downward to the next level
		 * (i.e. create a leaf node).
		 */
		else {
			int ret;
			int sz = __cvectc_size(p);

			ret = 0;
			if (__cvectc_path_decompress((struct cvcentry *)l, id, l->id, val)) return -1;
			if (__cvectc_upper_thresh(sz, __cvectc_nsubdirs(p))) {
				ret = __cvectc_level_compress(p, id);
				if (ret < 0) return -1;
			}
		}
		assert(cvectc_lookup(v, id) == val);
		cvectc_stats_nent(1);

		return 0;
	}
	/* else: no prefix match */

	/* ...otherwise do a long-form lookup through the structure of the tree */
	e = &v->d;
	do {
		struct cvcdir *d = __cvc_dir(e);

		/* find the first level to not match the prefix */
		if (!__cvectc_prefix_match(id, l->id, d->ignore)) {
			if (__cvectc_path_decompress((struct cvcentry *)d, id, l->id, val)) return -1;
			break;
		}
		e = __cvectc_next_lvl(d, id);
	} while (!__cvc_isleaf(e));
	assert(cvectc_lookup(v, id) == val);

	cvectc_stats_nent(1);
	return 0;
}

static inline int
__cvectc_nentries(struct cvcdir *d, struct cvcentry **entry)
{
	int              i, cnt = 0;
	struct cvcentry *e;
	int              sz;

	sz = __cvectc_size(d);
	assert(d && !__cvc_isleaf((struct cvcentry *)d));
	assert(sz >= CVECTC_MIN_ORDER);
	e = d->next;
	for (i = 0; i < sz; i++) {
		if (!__cvc_isleaf(&e[i]) || __cvc_ispresent(__cvc_leaf(&e[i]))) {
			*entry = &e[i];
			cnt++;
		}
	}

	return cnt;
}

static inline int
__cvectc_nsubdirs(struct cvcdir *d)
{
	int              i, cnt = 0;
	struct cvcentry *e;
	int              sz;

	sz = __cvectc_size(d);
	assert(d && !__cvc_isleaf((struct cvcentry *)d));
	assert(sz >= CVECTC_MIN_ORDER);
	e = d->next;
	for (i = 0; i < sz; i++) {
		if (!__cvc_isleaf(&e[i])) cnt++;
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
__cvectc_nentries_children(struct cvcdir *d, int *tot_sz)
{
	int              i, cnt = 0;
	struct cvcentry *e;
	int              sz = __cvectc_size(d);

	assert(d && !__cvc_isleaf((struct cvcentry *)d));
	assert(sz >= CVECTC_MIN_ENTRIES); /* check entries vs. order */
	assert(tot_sz);
	*tot_sz = 0;
	e       = d->next;
	for (i = 0; i < sz; i++) {
		if (!__cvc_isleaf(&e[i])) {
			struct cvcentry *e;
			struct cvcdir *  d = __cvc_dir(&e[i]);

			*tot_sz += __cvectc_size(d);
			cnt += __cvectc_nentries(d, &e);
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
	struct cvcentry *c, *p;
	int              sz;

	assert(d && !__cvc_isleaf((struct cvcentry *)d) && __cvectc_nentries(d, &p) == 0);

	c  = d->next;
	sz = __cvectc_size(d);
	CVECTC_FREE(c, sz);
	__cvectc_leaf_init_empty((struct cvcentry *)d, id);
	cvectc_stats_node(-1 * sz);
}

static void
cvectc_del(struct cvectc *v, u32_t id)
{
	struct cvcentry *e, *s; /* entry and saved entry */
	struct cvcdir *  p;     /* previous level */
	struct cvcleaf * l;
	int              nent;

	e = &v->d;
	l = __cvectc_lookup_leaf_prev(e, id, &p);
	assert(l);
	/* remove the item, but maintain the prefix */
	__cvectc_leaf_init_empty((struct cvcentry *)l, id);
	/* No previous level?  Nothing to compress: we're done. */
	if (p == __cvc_dir(&v->d)) {
		cvectc_stats_nent(-1);
		return;
	}

	nent = __cvectc_nentries(p, &s);
	/*
	 * If there were only one entry in the node, it should have
	 * been compressed!  Thus, check for this condition.
	 */
	if (nent == 0) __cvectc_path_compress(p, id);
	/*
	 * One entry in the subtree: kill the subtree, and make the
	 * reference to the subtree, reference to the entry instead.
	 */
	else if (nent == 1) {
		struct cvcentry saved; //, *t;

		//		assert(__cvc_isleaf(s) || __cvectc_nentries(__cvc_dir(s), &t) > 1);
		/* save the active entry */
		memcpy(&saved, s, sizeof(struct cvcentry));
		__cvectc_leaf_init_empty(s, id);
		/* get rid of the subtree */
		__cvectc_path_compress(p, id);
		/* put the saved entry in its place */
		memcpy(p, &saved, sizeof(struct cvcentry));
	}

	cvectc_stats_nent(-1);
	return;
}

#endif
