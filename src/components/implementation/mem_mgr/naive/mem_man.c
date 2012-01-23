/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Completely rewritten to use a sane data-structure based on the L4
 * mapping data-base -- Gabriel Parmer, gparmer@gwu.edu, 2011.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * I do _not_ use "embedded mapping nodes" here.  That is, I don't
 * embed the mapping nodes into the per-component "page tables" that
 * are used to look up individual mappings in each component.
 * Additionally, instead of the conventional implementation that has
 * these page table structures point to the frame structure that is
 * the base of the mapping tree, we point directly to the mapping to
 * avoid the O(N) cost when mapping where N is the number of nodes in
 * a mapping tree.  The combination of these design decisions means
 * that we might use more memory and have a few more data cache line
 * accesses.  We use a slab allocator to avoid excessive memory usage
 * for allocating memory mapping structures.  However, we use a very
 * fast (and predictable) lookup structure to perform the (component,
 * address)->mapping lookup.  Unfortunately the memory overhead of
 * that is significant (2 pages per component in the common case).
 * See cvectc.h for an alternative that trades (some) speed for memory
 * usage.
 */

/* 
 * FIXME: locking!
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <print.h>

#include <cos_list.h>
#include "../../sched/cos_sched_sync.h"
/* #define LOCK()  */
/* #define UNLOCK()  */
#define LOCK() if (cos_sched_lock_take()) assert(0);
#define UNLOCK() if (cos_sched_lock_release()) assert(0);

#include <mem_mgr.h>

/***************************************************/
/*** Data-structure for tracking physical memory ***/
/***************************************************/

struct frame {
	union {
		int nmaps;
		struct frame *free;
	} c;
} frames[COS_MAX_MEMORY];
struct frame *freelist;

static inline int  frame_index(struct frame *f) { return f-frames; }
static inline int  frame_nrefs(struct frame *f) { return f->c.nmaps; }
static inline void frame_ref(struct frame *f)   { f->c.nmaps++; }

static inline struct frame *
frame_alloc(void)
{
	struct frame *f = freelist;

	if (!f) return NULL;
	freelist = f->c.free;
	f->c.nmaps = 0;

	return f;
}

static inline void 
frame_deref(struct frame *f)
{ 
	f->c.nmaps--; 
	if (f->c.nmaps == 0) {
		f->c.free = freelist;
		freelist = f;
	}
}

static void
frame_init(void)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY-1 ; i++) {
		frames[i].c.free = &frames[i+1];
	}
	frames[COS_MAX_MEMORY-1].c.free = NULL;
	freelist = &frames[0];
}

/* <<<<<<< HEAD */
/* #define CACHE_SIZE 256 */
/* static struct mapping_cache { */
/* 	spdid_t spdid; */
/* 	vaddr_t addr; */
/* 	struct mem_cell *cell; */
/* 	int alias_num; */
/* } cache[CACHE_SIZE]; */
/* int cache_head = 0; */

/* static inline struct mapping_cache *cache_lookup(spdid_t spdid, vaddr_t addr) */
/* { */
/* 	int i; */

/* 	for (i = 0 ; i < CACHE_SIZE ; i++) { */
/* 		struct mapping_cache *c = &cache[i]; */
/* 		if (c->spdid == spdid && c->addr == addr) { */
/* 			assert(c->cell); */
/* 			return c; */
/* 		} */
/* 	} */
/* 	return NULL; */
/* } */

/* static inline void cache_add(spdid_t spdid, vaddr_t addr, struct mem_cell *mc, int alias) */
/* { */
/* 	struct mapping_cache *c = &cache[cache_head]; */
/* 	assert(cache_head < CACHE_SIZE); */
/* 	assert(mc); */
/* 	assert(spdid > 0); */

/* 	c->spdid = spdid; */
/* 	c->addr = addr; */
/* 	c->cell = mc; */
/* 	c->alias_num = alias; */
/* 	cache_head = (cache_head + 1) == CACHE_SIZE ? 0 : cache_head + 1; */
/* } */

/* static inline void cache_remove(struct mapping_cache *entry) */
/* { */
/* 	assert(entry); */

/* 	entry->spdid = 0; */
/* 	cache_head = entry-cache; */
/* } */

/* static inline struct mem_cell *find_cell(spdid_t spd, vaddr_t addr, int *alias, int use_cache) */
/* { */
/* 	int i, j; */
/* 	static int last_found = 0; */
/* 	int start_looking; */
/* 	struct mapping_cache *entry; */

/* 	if (likely(use_cache)) { */
/* 		entry = cache_lookup(spd, addr); */
/* 		if (entry) { */
/* 			*alias = entry->alias_num; */
/* 			return entry->cell; */
/* 		} */
/* 	} */
	
/* 	start_looking = last_found - 150; */
/* 	if (start_looking < 0) start_looking = 0; */

/* 	for (i = start_looking ; i < COS_MAX_MEMORY ; i++) { */
/* 		struct mem_cell *c = &cells[i]; */

/* 		for (j = 0; j < MAX_ALIASES; j++) { */
/* 			if (c->map[j].owner_spd == spd &&  */
/* 			    c->map[j].addr      == addr) { */
/* 				*alias = j; */
/* 				last_found = i; */
/* 				if (entry) { */
/* 					assert(entry->alias_num == j); */
/* 					assert(entry->cell == c); */
/* 				} */
/* 				return c; */
/* 			} */
/* 		} */
/* 	} */
/* 	for (i = 0 ; i < start_looking ; i++) { */
/* 		struct mem_cell *c = &cells[i]; */

/* 		for (j = 0; j < MAX_ALIASES; j++) { */
/* 			if (c->map[j].owner_spd == spd &&  */
/* 			    c->map[j].addr      == addr) { */
/* 				*alias = j; */
/* 				last_found = i; */
/* 				if (entry) { */
/* 					assert(entry->alias_num == j); */
/* 					assert(entry->cell == c); */
/* 				} */
/* 				return c; */
/* 			} */
/* 		} */
/* ======= */

static inline void
mm_init(void)
{
	static int first = 1;
	if (unlikely(first)) {
		first = 0;
		frame_init();
	}
}

/*************************************/
/*** Memory allocation shenanigans ***/
/*************************************/

static inline struct frame *frame_alloc(void);
static inline int frame_index(struct frame *f);
static inline void *
__page_get(void)
{
	void *hp = cos_get_vas_page();
	struct frame *f = frame_alloc();

	assert(hp && f);
	frame_ref(f);
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), (vaddr_t)hp, frame_index(f))) {
		BUG();
	}
	return hp;
}
#define CPAGE_ALLOC() __page_get()
#include <cpage_alloc.h>

#define CSLAB_ALLOC(sz)   cpage_alloc()
#define CSLAB_FREE(x, sz) cpage_free(x)
#include <cslab.h>

/* <<<<<<< HEAD */
/* 	cache_add(spd, addr, c, 0); */

/* 	return addr; */
/* err: */
/* 	return 0; */
/* ======= */
#define CVECT_ALLOC() cpage_alloc()
#define CVECT_FREE(x) cpage_free(x)
#include <cvect.h>

/**********************************************/
/*** Virtual address tracking per component ***/
/**********************************************/

CVECT_CREATE_STATIC(comps);
struct comp_vas {
	int nmaps;
	cvect_t *pages;
};
CSLAB_CREATE(cvas, sizeof(struct comp_vas));

static struct comp_vas *
cvas_lookup(spdid_t spdid)
{ return cvect_lookup(&comps, spdid); }

static struct comp_vas *
cvas_alloc(spdid_t spdid)
{
	struct comp_vas *cv;

	assert(!cvas_lookup(spdid));
	cv = cslab_alloc_cvas();
	if (!cv) goto done;
	cv->pages = cvect_alloc();
	if (!cv->pages) goto free;
	cvect_init(cv->pages);
	cvect_add(&comps, cv, spdid);
	cv->nmaps = 0;
done:
	return cv;
free:
	cslab_free_cvas(cv);
	cv = NULL;
	goto done;

}

static void
cvas_ref(struct comp_vas *cv)
{
/* <<<<<<< HEAD */
/* 	int alias = -1, i; */
/* 	struct mem_cell *c; */
/* 	struct mapping_info *base; */

/* 	c = find_cell(s_spd, s_addr, &alias, 1); */

/* 	if (-1 == alias) {printc("WTF \n");goto err;} */
/* 	assert(alias >= 0 && alias < MAX_ALIASES); */
/* 	base = c->map; */
/* 	for (i = 0 ; i < MAX_ALIASES ; i++) { */
/* 		if (alias == i || base[i].owner_spd != 0 || base[i].addr != 0) { */
/* 			continue; */
/* 		} */
		
/* 		if (cos_mmap_cntl(COS_MMAP_GRANT, 0, d_spd, d_addr, cell_index(c))) { */
/* 			printc("mm: could not alias page @ %x to spd %d from %x(%d)\n",  */
/* 			       (unsigned int)d_addr, (unsigned int)d_spd, (unsigned int)s_addr, (unsigned int)s_spd); */
/* 			goto err; */
/* 		} */
/* 		base[i].owner_spd = d_spd; */
/* 		base[i].addr = d_addr; */
/* 		base[i].parent = alias; */
/* 		c->naliases++; */
/* 		cache_add(d_spd, d_addr, c, i); */

/* 		return d_addr; */
/* 	} */
/* 	/\* no available alias slots! *\/ */
/* err: */
/* 	return 0; */
/* ======= */
	assert(cv);
	cv->nmaps++;
}

static void 
cvas_deref(struct comp_vas *cv)
{
	assert(cv && cv->nmaps > 0);
	cv->nmaps--;
	if (cv->nmaps == 0) {
		cvect_free(cv->pages);
		cslab_free_cvas(cv);
	}
}

/**************************/
/*** Mapping operations ***/
/**************************/

struct mapping {
	u16_t   flags;
	spdid_t spdid;
	vaddr_t addr;

	struct frame *f;
	/* child and sibling mappings */
	struct mapping *p, *c, *_s, *s_;
} __attribute__((packed));
CSLAB_CREATE(mapping, sizeof(struct mapping));

static void
mapping_init(struct mapping *m, spdid_t spdid, vaddr_t a, struct mapping *p, struct frame *f)
{
/* <<<<<<< HEAD */
/* 	int alias, i; */
/* 	struct mem_cell *mc; */
/* 	struct mapping_info *mi; */

/* 	mc = find_cell(spd, addr, &alias, 1); */
	
/* 	if (!mc) { */
/* 		/\* FIXME: add return codes to this call *\/ */
/* 		return; */
/* 	} */
/* 	mi = mc->map; */
/* 	for (i = 0 ; i < MAX_ALIASES ; i++) { */
/* 		int idx; */
/* 		struct mapping_cache *cache; */

/* 		if (i == alias || !mi[i].owner_spd ||  */
/* 		    !is_descendent(mi, alias, i)) continue; */
/* 		idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, mi[i].owner_spd,  */
/* 				    mi[i].addr, 0); */
/* 		assert(&cells[idx] == mc); */
/* 		if ((cache = cache_lookup(mi[i].owner_spd, mi[i].addr))) cache_remove(cache); */

/* 		/\* mark page as removed *\/ */
/* 		mi[i].addr = 0; */
/* 		mc->naliases--; */
/* ======= */
	assert(m && f);
	INIT_LIST(m, _s, s_);
	m->f     = f;
	m->flags = 0;
	m->spdid = spdid;
	m->addr  = a;
	m->p     = p;
	if (p) {
		m->flags = p->flags;
		if (!p->c) p->c = m;
		else       ADD_LIST(p->c, m, _s, s_);
	}
}

static struct mapping *
mapping_lookup(spdid_t spdid, vaddr_t addr)
{
	struct comp_vas *cv = cvas_lookup(spdid);

	if (!cv) return NULL;
	return cvect_lookup(cv->pages, addr >> PAGE_SHIFT);
}

/* Make a child mapping */
static struct mapping *
mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to)
{
	struct comp_vas *cv = cvas_lookup(dest);
	struct mapping *m = NULL;
	long idx = to >> PAGE_SHIFT;

	assert(!p || p->f == f);
	assert(dest && to);
	/* no vas structure for this spd yet... */
	if (!cv) {
		cv = cvas_alloc(dest);
		if (!cv) goto done;
		assert(cv == cvas_lookup(dest));
	}
	assert(cv->pages);
	if (cvect_lookup(cv->pages, idx)) goto collision;
	cvas_ref(cv);
	m = cslab_alloc_mapping();
	if (!m) goto no_mapping;

	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, dest, to, frame_index(f))) {
		BUG();
		goto no_mapping;
	}
	mapping_init(m, dest, to, p, f);
	assert(!p || frame_nrefs(f) > 0);
	frame_ref(f);
	if (cvect_add(cv->pages, m, idx)) BUG();
done:
	return m;
no_mapping:
	cvas_deref(cv);
collision:
	m = NULL;
	goto done;
}

/* Take all decedents, return them in a list. */
static struct mapping *
__mapping_linearize_decendents(struct mapping *m)
{
	struct mapping *first, *last, *c, *gc;
	
	first = c = m->c;
	m->c = NULL;
	if (!c) return NULL;
	do {
		last = LAST_LIST(first, _s, s_);
		c->p = NULL;
		gc = c->c;
		c->c = NULL;
		/* add the grand-children onto the end of our list of decedents */
		if (gc) ADD_LIST(last, gc, _s, s_);
		c = FIRST_LIST(c, _s, s_);
	} while (first != c);
	
	return first;
}

static void
__mapping_destroy(struct mapping *m)
{
	struct comp_vas *cv;
	int idx;

	assert(m);
	assert(EMPTY_LIST(m, _s, s_));
	assert(m->p == NULL && m->c == NULL);
	cv = cvas_lookup(m->spdid);

	assert(cv && cv->pages);
	assert(m == cvect_lookup(cv->pages, m->addr >> PAGE_SHIFT));
	cvect_del(cv->pages, m->addr >> PAGE_SHIFT);
	cvas_deref(cv);

	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, m->spdid, m->addr, 0);
	assert(idx == frame_index(m->f));
	frame_deref(m->f);
	cslab_free_mapping(m);
}

static void
mapping_del_children(struct mapping *m)
{
/* <<<<<<< HEAD */
/* 	int alias = -1; */
/* 	long idx; */
/* 	struct mem_cell *mc; */
/* 	struct mapping_info *mi; */
/* 	struct mapping_cache *cache_entry; */

/* 	mman_revoke_page(spd, addr, flags); */

/* 	cache_entry = cache_lookup(spd, addr); */
/* 	if (cache_entry) { */
/* 		alias = cache_entry->alias_num; */
/* 		mc = cache_entry->cell; */
/* 		cache_remove(cache_entry); */
/* 	} else { */
/* 		mc = find_cell(spd, addr, &alias, 0); */
/* 	} */
/* 	if (!mc) { */
/* 		/\* FIXME: add return codes to this call *\/ */
/* 		return; */
/* 	} */
/* 	mi = mc->map; */
/* 	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, mi[alias].owner_spd,  */
/* 			    mi[alias].addr, 0); */
/* 	assert(&cells[idx] == mc); */
/* 	mi[alias].addr = 0; */
/* 	mi[alias].owner_spd = 0; */
/* 	mi[alias].parent = 0; */
/* 	mc->naliases--; */
/* 	if (cache_entry) cache_remove(cache_entry); */

/* 	return; */
/* ======= */
	struct mapping *d, *n; 	/* decedents, next */

	assert(m);
	d = __mapping_linearize_decendents(m);
	while (d) {
		n = FIRST_LIST(d, _s, s_);
		REM_LIST(d, _s, s_);
		__mapping_destroy(d);
		d = (n == d) ? NULL : n;
	}
	assert(!m->c);
}

static void
mapping_del(struct mapping *m)
{
	assert(m);
	mapping_del_children(m);
	assert(!m->c);
	if (m->p && m->p->c == m) {
		if (EMPTY_LIST(m, _s, s_)) m->p->c = NULL;
		else                       m->p->c = FIRST_LIST(m, _s, s_);
		m->p = NULL;
	}
	REM_LIST(m, _s, s_);
	__mapping_destroy(m);
}

/**********************************/
/*** Public interface functions ***/
/**********************************/

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct frame *f;
	struct mapping *m;
	vaddr_t ret = 0;

	LOCK();
	mm_init();
	f = frame_alloc();
	if (!f) goto done; 	/* -ENOMEM */
	frame_ref(f);
	m = mapping_crt(NULL, f, spd, addr);
	if (!m) goto dealloc;
	assert(m->addr == addr);
	assert(m->spdid == spd);
	assert(m == mapping_lookup(spd, addr));
	ret = m->addr;
done:
	UNLOCK();
	return m->addr;
dealloc:
	frame_deref(f);
	goto done;		/* -EINVAL */
}

vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct mapping *m, *n;
	vaddr_t ret = 0;

	LOCK();
	mm_init();
	m = mapping_lookup(s_spd, s_addr);
	if (!m) goto done; 	/* -EINVAL */
	n = mapping_crt(m, m->f, d_spd, d_addr);
	if (!n) goto done;

	assert(n->addr  == d_addr);
	assert(n->spdid == d_spd);
	assert(n->p     == m);
	ret = d_addr;
done:
	UNLOCK();
	return ret;
}

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	LOCK();
	mm_init();
	m = mapping_lookup(spd, addr);
	if (!m) {
		ret = -1;	/* -EINVAL */
		goto done;
	}
	mapping_del_children(m);
done:
	UNLOCK();
	return ret;
}

int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	LOCK();
	mm_init();
	m = mapping_lookup(spd, addr);
	if (!m) {
		ret = -1;	/* -EINVAL */
		goto done;
	}
	mapping_del(m);
done:
	UNLOCK();
	return ret;
}

void mman_print_stats(void) {}
