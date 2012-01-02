/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Completely rewritten to use a sane data-structure, Gabriel Parmer,
 * gparmer@gwu.edu, 2011.
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

#include <mem_mgr.h>

/*** Data-structure for tracking physical memory ***/
struct frame {
	union c {
		int nmaps;
		struct frame *free;
	}
} frames[COS_MAX_MEMORY];
struct frame *freelist;

static inline int  frame_index(struct frame *f)   { return f-frames; }
static inline void frame_nrefs(struct frame *f)   { return f->c.nmaps; }
static inline void frame_ref(struct frame *f)     { f->c.nmaps++; }

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

/**********************************************/
/*** Virtual address tracking per component ***/
/**********************************************/

COS_VECT_CREATE_STATIC(comps);
struct comp_vas {
	int nmaps;
	cos_vect_t *pages;
};

static struct comp_vas *
cvas_lookup(spdid_t spdid)
{ return cos_vect_lookup(&comps, spdid); }

static struct comp_vas *
cvas_alloc(spdid_t spdid)
{
	struct comp_vas *cv

	assert(!cvas_lookup(spdid));
	cv = malloc(sizeof(struct comp_vas));
	if (!cv) goto done;
	cv->pages = cos_vect_alloc();
	if (!cv->pages) goto free;
	cos_vect_init(cv->pages);
	cv->nmaps = 0;
done:
	return cv;
free:
	free(cv);
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
		cos_vect_free(cv->pages);
		free(cv);
	}
}

/**************************/
/*** Mapping operations ***/
/**************************/

struct mapping {
	int flags;
	spdid_t spdid;
	vaddr_t addr;

	struct frame *f;
	/* child and sibling mappings */
	struct mapping *p, *c, *_s, *s_;
} __attribute__((packed));

static void
mapping_init(struct mapping *m, struct mapping *p, struct frame *f)
{
	assert(m && f);
	INIT_LIST(m, _s, s_);
	m->p = p;
	m->f = f;
	m->flags = 0;
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
	return cos_vect_lookup(cv->pages, addr >> PAGE_SHIFT);
}

/* Make a child mapping */
static struct mapping *
mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to)
{
	struct comp_vas *cv = cvas_lookup(spdid);
	struct mapping *m;
	long idx = to >> PAGE_SHIFT;

	assert(!p || p->f == f);
	if (!cv) {
		cv = cvas_alloc(dest);
		if (!cv) return NULL;
	}
	cvas_ref(cv);
	if (cos_vect_lookup(cv->pages, idx)) goto no_mapping;
	m = malloc(sizeof(struct mapping));
	if (!m) goto no_mapping;

	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, dest, to, frame_index(f))) {
		BUG();
		goto no_mapping;
	}
	mapping_init(m, p, f);
	assert(frame_nrefs(f) > 0)
	frame_ref(f);
	if (cos_vect_add(cv->pages, m, idx)) BUG();
done:
	return m;
no_mapping:
	cvas_deref(cv);
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
		last = PREV_LIST(first, _s, s_);
		c->p = NULL;
		gc = c->c;
		c->c = NULL;
		/* add the grand-children onto the end of our list of decedents */
		LIST_ADD(last, gc, _s, s_);
		c = NEXT_LIST(c, _s, s_);
	} while (first != c);
	
	return first;
}

static void
__mapping_destroy(struct mapping *m)
{
	comp_vas *cv;
	int idx;

	assert(m);
	assert(EMPTY_LIST(m, _s, s_));
	assert(m->p == NULL && m->c == NULL);
	cv = cvas_lookup(m->spdid);
	assert(cv);
	cos_vect_del(cv->pages, m->addr >> PAGE_SHIFT);
	cvas_deref(cv);

	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, m->spdid, m->addr, 0);
	assert(idx == frame_index(m->f));
	frame_deref(m->f);
	free(m);
}

static void
mapping_del_children(struct mapping *m)
{
	struct mapping *d, *n; 	/* decedents, next */

	assert(m);
	d = mapping_linearize_decendents(m);
	while (d) {
		n = NEXT_LIST(d, _s, s_);
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
		else                       m->p->c = NEXT_LIST(m, _s, s_);
		m->p = NULL;
	}
	REM_LIST(m, _s, s_);
	__mapping_destroy(d);
}

/********************************/
/*** Public interface methods ***/
/********************************/

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct frame *f;
	struct mapping *m;

	mm_init();
	f = frame_alloc();
	if (!f) return 0;
	m = mapping_crt(NULL, f, spd, addr);
	if (!m) goto dealloc;
	assert(m->addr == addr);
	assert(m->spdid == spd);
done:
	return m->addr;
dealloc:
	frame_ref(f);
	frame_deref(f);
	goto done;
}

vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct mapping *m, *n;

	mm_init();
	m = mapping_lookup(s_spd, s_addr);
	if (!m) return 0;
	n = mapping_crt(m, m->f, d_spd, d_addr);
	if (!n) return 0;
	
	assert(n->addr == d_addr);
	assert(n->spdid == d_spdid);
	assert(n->p == m);
	return d_addr;
}

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;

	mm_init();
	m = mapping_lookup(spd, addr);
	if (!m) return -1;
	mapping_del_children(m);
}

int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;

	mm_init();
	m = mapping_lookup(spd, addr);
	if (!m) return -1;
	mapping_del(m);
}
