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
 *
 * Modified by Qi Wang, interwq@gwu.edu, 2014
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
#include "../../sched/cos_sched_ds.h"
#include "../../sched/cos_sched_sync.h"

#if NUM_CPU_COS > 1
#include <ck_spinlock.h>
ck_spinlock_ticket_t xcore_lock = CK_SPINLOCK_TICKET_INITIALIZER;

#define LOCK()   do { if (cos_sched_lock_take())  { assert(0); } ck_spinlock_ticket_lock_pb(&xcore_lock, 1); } while (0)
#define UNLOCK() do { ck_spinlock_ticket_unlock(&xcore_lock); if (cos_sched_lock_release()) { assert(0); } } while (0)
#else
#define LOCK()   do { if (cos_sched_lock_take())    { assert(0); } } while (0)
#define UNLOCK() do { if (cos_sched_lock_release()) { assert(0); } } while (0)
#endif

#include <mem_mgr.h>

/***************************************************/
/*** Data-structure for tracking physical memory ***/
/***************************************************/

struct mapping;
/* A tagged union, where the tag holds the number of maps: */
struct frame {
	int nmaps;
	union {
		struct mapping *m;  /* nmaps > 0 : root of all mappings */
		vaddr_t addr;	    /* nmaps = -1: local mapping */
		struct frame *free; /* nmaps = 0 : no mapping */
	} c;
} all_frames[COS_MAX_MEMORY + COS_KERNEL_MEMORY];

/* all_frames: user frames + kernel frames */

static struct frame *frames      = all_frames;
static struct frame *kern_frames = all_frames + COS_MAX_MEMORY;

struct frame *freelist;
struct frame *kern_freelist;

#define IS_USER_FRAME(f) ((f < kern_frames))

static inline int  frame_index(struct frame *f) {
	if (IS_USER_FRAME(f))
		return f-frames;
	else
		return f-kern_frames;
}
static inline int  frame_nrefs(struct frame *f) { return f->nmaps; }
static inline void frame_ref(struct frame *f)   { f->nmaps++; }

static inline struct frame *
frame_alloc(void)
{
	struct frame *f = freelist;

	if (!f) return NULL;
	freelist = f->c.free;
	f->nmaps = 0;
	f->c.m   = NULL;

	return f;
}

static inline struct frame *
kern_frame_alloc(void)
{
	struct frame *f = kern_freelist;

	if (!f) return NULL;
	kern_freelist = f->c.free;
	f->nmaps = 0;
	f->c.m   = NULL;

	return f;
}

static inline void
frame_free(struct frame *f)
{
	assert(f->nmaps == 0);
	f->c.free = freelist;
	freelist  = f;
}

static inline void
frame_deref(struct frame *f)
{
	assert(f->nmaps > 0);
	f->nmaps--;
	if (f->nmaps == 0) {
		if (IS_USER_FRAME(f)) {
			f->c.free = freelist;
			freelist  = f;
		} else {
			f->c.free = kern_freelist;
			kern_freelist  = f;
		}
	}
}

static void *init_vas = 0;
static int max_npages, max_npages_kern;
static void
init_frames(void)
{
	int i;
	max_npages = cos_pfn_cntl(COS_PFN_MAX_MEM, 0, 0, 0);
	max_npages_kern = cos_pfn_cntl(COS_PFN_MAX_MEM_KERN, 0, 0, 0);

	if (!init_vas) init_vas = cos_get_vas_page();

	/* zero out all frames: map in, mem_set, unmap. */
	for (i = 0 ; i < max_npages ; i++) {
		if (cos_mmap_cntl(COS_MMAP_GRANT, MAPPING_RW, cos_spd_id(), (vaddr_t)init_vas, i)) {
			goto err;
		}
		memset(init_vas, 0, PAGE_SIZE);
		if (i != cos_mmap_cntl(COS_MMAP_REVOKE, 0, cos_spd_id(), (vaddr_t)init_vas, 0)) {
			goto err;
		}
		cos_mmap_cntl(COS_MMAP_TLBFLUSH, 0, cos_spd_id(), 0, 0);
	}

	return;
err:
	BUG();
	return;
}

static void
frame_init(void)
{
	int i;

	init_frames();

	/* User frames. */
	for (i = 0 ; i < max_npages-1 ; i++) {
		frames[i].c.free = &frames[i+1];
		frames[i].nmaps  = 0;
	}
	frames[max_npages-1].c.free = NULL;
	freelist = &frames[0];

	/* Next, kernel frames. */
	for (i = 0 ; i < max_npages_kern-1 ; i++) {
		kern_frames[i].c.free = &kern_frames[i+1];
		kern_frames[i].nmaps  = 0;
	}
	kern_frames[max_npages_kern-1].c.free = NULL;
	kern_freelist = &kern_frames[0];
}

#define NREGIONS 4

extern struct cos_component_information cos_comp_info;

static inline void
mm_init(void)
{
	printc("core %ld: mm init as thread %d\n", cos_cpuid(), cos_get_thd_id());

	/* Expanding VAS. */
	printc("mm expanding %lu MBs @ %p\n", (NREGIONS-1) * round_up_to_pgd_page(1) / 1024 / 1024,
	       (void *)round_up_to_pgd_page((unsigned long)&cos_comp_info.cos_poly[1]));
	if (cos_vas_cntl(COS_VAS_SPD_EXPAND, cos_spd_id(),
			 round_up_to_pgd_page((unsigned long)&cos_comp_info.cos_poly[1]),
			 (NREGIONS-1) * round_up_to_pgd_page(1))) {
		printc("MM could not expand VAS\n");
		BUG();
	}

	frame_init();

	printc("core %ld: mm init done\n", cos_cpuid());
}

/*************************************/
/*** Memory allocation shenanigans ***/
/*************************************/

static inline void *
__page_get(void)
{
	void *hp = cos_get_vas_page();
	struct frame *f = frame_alloc();

	assert(hp && f);
	frame_ref(f);
	f->nmaps  = -1; 	 /* belongs to us... */
	f->c.addr = (vaddr_t)hp; /* ...at this address */
	if (cos_mmap_cntl(COS_MMAP_GRANT, MAPPING_RW, cos_spd_id(), (vaddr_t)hp, frame_index(f))) {
		printc("grant @ %p for frame %d\n", hp, frame_index(f));
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
	int nmaps, spdid;
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
	cv->spdid = spdid;
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
		cvect_del(&comps, cv->spdid);
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
mapping_init(struct mapping *m, spdid_t spdid, vaddr_t a, struct mapping *p, struct frame *f, int flags)
{
	assert(m && f);
	INIT_LIST(m, _s, s_);
	m->f     = f;
	m->flags = flags;
	m->spdid = spdid;
	m->addr  = a;
	m->p     = p;
	m->c     = NULL;
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
mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to, int flags)
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
	if (!m) goto collision;

	if (cos_mmap_cntl(COS_MMAP_GRANT, flags, dest, to, frame_index(f))) {
		printc("mem_man naive: could not grant at %x:%x\n", dest, to);
		goto no_mapping;
	}
	mapping_init(m, dest, to, p, f, flags);
	assert(!p || frame_nrefs(f) > 0);
	frame_ref(f);
	assert(frame_nrefs(f) > 0);
	if (cvect_add(cv->pages, m, idx)) BUG();
done:
	return m;
no_mapping:
	cslab_free_mapping(m);
collision:
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
		last = LAST_LIST(first, _s, s_);
		c->p = NULL;
		gc = c->c;
		c->c = NULL;
		if (gc) APPEND_LIST(last, gc, _s, s_);
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
	assert(m->p == NULL);
	assert(m->c == NULL);
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
	}
	m->p = NULL;
	REM_LIST(m, _s, s_);
	__mapping_destroy(m);
}

/**********************************/
/*** Public interface functions ***/
/**********************************/

int __mman_fork_spd(spdid_t spd, u32_t s_spd_d_spd, vaddr_t base, u32_t len)
{
	struct mapping *m, *n;
	int ret = 0;
	spdid_t s_spd, d_spd;
	vaddr_t s_addr;

	s_spd = s_spd_d_spd >> 16;
	d_spd = s_spd_d_spd & 0xFFFF;

	/* do we need locking? what if m gets revoked after lookup? */
	/* note: children mappings stay with s_spd, but what if they
	 * are needed in a dependent of d_spd? */

	for ( s_addr = base ; !ret && s_addr < s_addr + len; s_addr += PAGE_SIZE ) {
		if (!(m = mapping_lookup(s_spd, s_addr))) continue;
		if ((n = mapping_lookup(d_spd, s_addr))) continue;
		if (!m->p) { /* no parent, create a new mapping */
			if (s_addr != mman_get_page(d_spd, s_addr, m->flags)) ret = -EFAULT;
			if (m->c) {
				/* children mappings exist, what to do? */

			}
		} else { /* this is an alias, re-alias from p */
			/* FIXME: shouldn't share RW memory? how to do
			 * this sanely? maybe m->p->spdid should do it? */
			if (s_addr != mman_alias_page(m->p->spdid, m->p->addr, d_spd, s_addr, m->flags)) ret = -EINVAL;
		}
#if defined(DEBUG)
		//printc("mman_fork: recreated mapping in %d at %x\n", d_spd, s_addr);
#endif
	}

	return ret;
}

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct frame *f;
	struct mapping *m = NULL;
	vaddr_t ret = 0;

	LOCK();
	if (flags & MAPPING_KMEM)
		f = kern_frame_alloc();
	else
		f = frame_alloc();
	if (!f) goto done; 	/* -ENOMEM */

	assert(frame_nrefs(f) == 0);
	m = mapping_crt(NULL, f, spd, addr, flags);
	if (!m) goto dealloc;
	f->c.m = m;
	assert(m->addr == addr);
	assert(m->spdid == spd);
	assert(m == mapping_lookup(spd, addr));
	ret = m->addr;
done:
	UNLOCK();
	return ret;
dealloc:
	frame_free(f);
	goto done;		/* -EINVAL */
}

vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr)
{
	struct mapping *m, *n;
	vaddr_t ret = 0;
	spdid_t d_spd;
	int flags;

	d_spd = d_spd_flags >> 16;
	flags = d_spd_flags & 0xFFFF;
	LOCK();
	m = mapping_lookup(s_spd, s_addr);
	if (!m) goto done; 	/* -EINVAL */
	n = mapping_crt(m, m->f, d_spd, d_addr, flags);
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

void mman_release_all(void)
{
	int i;

	LOCK();
	/* kill all mappings in other components */
	for (i = 0 ; i < max_npages ; i++) {
		struct frame *f = &frames[i];
		struct mapping *m;

		if (frame_nrefs(f) <= 0) continue;
		m = f->c.m;
		assert(m);
		mapping_del(m);
	}
	/* kill local mappings */
	/* for (i = 0 ; i < max_npages ; i++) { */
	/* 	struct frame *f = &frames[i]; */
	/*      int idx; */

	/* 	if (frame_nrefs(f) >= 0) continue; */
	/* 	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, cos_spd_id(), f->c.addr, 0); */
	/* 	assert(idx == frame_index(f)); */
	/* } */
	UNLOCK();
}

/*******************************/
/*** The base-case scheduler ***/
/*******************************/

#include <sched_hier.h>

int  sched_init(void)   { return 0; }

extern void parent_sched_exit(void);

PERCPU_ATTR(static volatile, int, initialized_core); /* record the cores that still depend on us */

void
sched_exit(void)
{
	if (cos_cpuid() == INIT_CORE) {
		int i;
		/* The init core waiting for all cores to exit. */
		for (i = 0; i < NUM_CPU ; i++)
			if (*PERCPU_GET_TARGET(initialized_core, i)) i = 0;
		/* Don't delete the memory until all cores exit */
		mman_release_all();
		*PERCPU_GET(initialized_core) = 0;
	} else {
		/* No one should exit before all cores are done. We'll
		 * disable hw interposition once exit. */
		*PERCPU_GET(initialized_core) = 0;

		while (*PERCPU_GET_TARGET(initialized_core, 0)) ;
	}
	parent_sched_exit();
}

int sched_isroot(void) { return 1; }

int
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff) { BUG(); return 0; }

extern int parent_sched_child_cntl_thd(spdid_t spdid);

int
sched_child_cntl_thd(spdid_t spdid)
{
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) BUG();
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, cos_get_thd_id(), spdid)) BUG();

	return 0;
}

int
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) { BUG(); return 0; }

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	/* printc("cpu %ld: thd %d in mem_mgr init. args %d, %p, %p, %p\n", */
	/*        cos_cpuid(), cos_get_thd_id(), t, arg1, arg2, arg3); */
	switch (t) {
	case COS_UPCALL_THD_CREATE:
		if (cos_cpuid() == INIT_CORE) {
			int i;
			for (i = 0; i < NUM_CPU; i++)
				*PERCPU_GET_TARGET(initialized_core, i) = 0;
			mm_init();
		} else {
			/* Make sure that the initializing core does
			 * the initialization before any other core
			 * progresses */
			while (*PERCPU_GET_TARGET(initialized_core, INIT_CORE) == 0) ;
		}
		*PERCPU_GET(initialized_core) = 1;
		break;
	default:
		BUG(); return;
	}

	return;
}
