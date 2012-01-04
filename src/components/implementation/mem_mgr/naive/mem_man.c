/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Completely rewritten to use a sane data-structure based on the L4
 * mapping data-base -- Gabriel Parmer, gparmer@gwu.edu, 2011.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
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
	struct mapping *m;
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

	mm_init();
	f = frame_alloc();
	if (!f) return 0; 	/* -ENOMEM */
	frame_ref(f);
	m = mapping_crt(NULL, f, spd, addr);
	if (!m) goto dealloc;
	assert(m->addr == addr);
	assert(m->spdid == spd);
	assert(m == mapping_lookup(spd, addr));

	return m->addr;
dealloc:
	frame_deref(f);
	return 0;		/* -EINVAL */
}

vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct mapping *m, *n;

	mm_init();
	m = mapping_lookup(s_spd, s_addr);
	if (!m) return 0; 	/* -EINVAL */
	n = mapping_crt(m, m->f, d_spd, d_addr);
	if (!n) return 0;

	assert(n->addr == d_addr);
	assert(n->spdid == d_spd);
	assert(n->p == m);
	return d_addr;
}

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;

	mm_init();
	m = mapping_lookup(spd, addr);
	if (!m) return -1;	/* -EINVAL */
	mapping_del_children(m);

	return 0;
}

int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;

	mm_init();
	m = mapping_lookup(spd, addr);
	if (!m) return -1;	/* -EINVAL */
	mapping_del(m);

	return 0;
}

void mman_print_stats(void) {}
