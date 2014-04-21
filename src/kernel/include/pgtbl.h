/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef PGTBL_H
#define PGTBL_H

/***
 * This is a test of the ertrie code that essentially emulates the
 * special constraints of a page-table to make sure they work for that
 * specific case.
 */

#include "ertrie.h"
#include "shared/util.h"
#include "captbl.h"

enum {
	PGTBL_PRESENT      = 1,
	PGTBL_WRITABLE     = 1<<1,
	PGTBL_USER         = 1<<2,
	PGTBL_WT           = 1<<3, 	/* write-through caching */
	PGTBL_NOCACHE      = 1<<4, 	/* caching disabled */
	PGTBL_ACCESSED     = 1<<5,
	PGTBL_MODIFIED     = 1<<6,
	PGTBL_SUPER        = 1<<7, 	/* super-page (4MB on x86-32) */
	PGTBL_GLOBAL       = 1<<8,
	PGTBL_COSFRAME     = 1<<9,
	PGTBL_INTERN_DEF   = PGTBL_PRESENT|PGTBL_WRITABLE|PGTBL_USER| 
	                     PGTBL_ACCESSED|PGTBL_MODIFIED,
};

#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH         2
#define PGTBL_ORD           10

/* 
 * Use the passed in page, but make sure that we only use the passed
 * in page once.
 */
static inline void *
__pgtbl_a(void *d, int sz, int leaf) 
{ 
	void **i = d, *p;

	(void)leaf;
	assert(sz == PAGE_SIZE);
	if (unlikely(!*i)) return NULL;
	p = *i;
	*i = NULL;
	return p;
}
static struct ert_intern *
__pgtbl_get(struct ert_intern *a, void *accum, int isleaf)
{ 
	/* don't use | here as we only want the pte flags */
	*(u32_t*)accum = (((u32_t)a->next) & PGTBL_FLAG_MASK); 
	if (!isleaf) return chal_pa2va((void*)((((u32_t)a->next) & PGTBL_FRAME_MASK))); 
	/* return pa when it's leaf */
	else         return (struct ert_intern *)(((u32_t)a->next) & PGTBL_FRAME_MASK);
}
static int __pgtbl_isnull(struct ert_intern *a, void *accum, int isleaf) 
{ (void)isleaf; (void)accum; return !(((u32_t)(a->next)) & (PGTBL_PRESENT|PGTBL_COSFRAME)); }
static void __pgtbl_init(struct ert_intern *a, int isleaf) 
{ 
	if (isleaf) return; 
	a->next = NULL;
}

static inline int __pgtbl_setleaf(struct ert_intern *a, void *v)
{
	u32_t old, new;

	old = (u32_t)(a->next);
	new = (u32_t)(((u32_t)v & PGTBL_FRAME_MASK) | ((u32_t)v & PGTBL_FLAG_MASK));

	if (cos_cas((unsigned long *)a, old, new)) return 0;
	else return -1;
}
/* Note:  We're just using pre-defined default flags for internal (pgd) entries */
static void __pgtbl_set(struct ert_intern *a, void *v, void *accum, int isleaf) 
{ 
	(void)accum; assert(!isleaf);
	if (v) a->next = (void*)((u32_t)chal_va2pa((void*)((u32_t)v & PGTBL_FRAME_MASK)) | PGTBL_INTERN_DEF); 
	else   a->next = NULL;
}
static inline void *__pgtbl_getleaf(struct ert_intern *a, void *accum)
{ if (unlikely(!a)) return NULL; return __pgtbl_get(a, accum, 1); }

ERT_CREATE(__pgtbl, pgtbl, PGTBL_DEPTH, PGTBL_ORD, sizeof(int*), PGTBL_ORD, sizeof(int*), NULL, \
	   __pgtbl_init, __pgtbl_get, __pgtbl_isnull, __pgtbl_set,	\
	   __pgtbl_a, __pgtbl_setleaf, __pgtbl_getleaf, ert_defresolve);

/* make it an opaque type...not to be touched */
typedef struct pgtbl * pgtbl_t; 

/* identical to the capability structure */
struct cap_pgtbl {
	struct cap_header h;
	pgtbl_t pgtbl;
	u32_t lvl; 		/* what level are the pgtbl nodes at? */
};

static pgtbl_t pgtbl_alloc(void *page) 
{ return __pgtbl_alloc(&page); }

static void
pgtbl_init_pte(void *pte)
{
	int i;
	unsigned long *vals = pte;

	for (i = 0 ; i < 1<<PGTBL_ORD ; i++) vals[i] = 0;
}

static int 
pgtbl_intern_expand(pgtbl_t pt, u32_t addr, void *pte, u32_t flags)
{
	unsigned long accum = (unsigned long)flags;
	int ret;

	/* NOTE: flags currently ignored. */

	assert(pt);
	assert((PGTBL_FLAG_MASK & (u32_t)pte) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	if (!pte) return -EINVAL;
	ret = __pgtbl_expandn(pt, (unsigned long)(addr >> PGTBL_PAGEIDX_SHIFT), 
			      PGTBL_DEPTH, &accum, &pte, NULL);
	if (!ret && pte) return -EEXIST; /* no need to expand */
	assert(!(ret && !pte));		 /* error and used memory??? */

	return ret;
}

static void *
pgtbl_intern_prune(pgtbl_t pt, u32_t addr)
{
	unsigned long accum = 0, *pgd;
	void *page;

	assert(pt);
	assert((PGTBL_FLAG_MASK & (u32_t)addr) == 0);

	pgd = __pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT), (u32_t)addr >> PGTBL_PAGEIDX_SHIFT, 1, &accum);
	if (!pgd) return NULL;
	page = __pgtbl_get((struct ert_intern *)pgd, &accum, 0);
	accum = 0;
	__pgtbl_set((struct ert_intern *)pgd, NULL, &accum, 0);

	return page;
}

static void *
pgtbl_get_pgd(pgtbl_t pt, u32_t addr)
{
	unsigned long accum = 0;

	assert(pt);
	return __pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT), (u32_t)addr >> PGTBL_PAGEIDX_SHIFT, 1, &accum);
}

static int
pgtbl_check_pgd_absent(pgtbl_t pt, u32_t addr)
{
	return __pgtbl_isnull(pgtbl_get_pgd(pt, (u32_t)addr), 0, 0);
}

/* 
 * FIXME: need to change this to _not_ add over an already non-null
 * entry.
 */
static int
pgtbl_mapping_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	unsigned long accum = 0, *pte = NULL;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	return __pgtbl_expandn(pt, addr >> PGTBL_PAGEIDX_SHIFT, 
			       PGTBL_DEPTH+1, &accum, &pte, (void*)(page | flags));
}

/* This function updates flags of an existing mapping. */
static int
pgtbl_mapping_mod(pgtbl_t pt, u32_t addr, u32_t flags, u32_t *prevflags)
{
	struct ert_intern *pte;
	u32_t orig_v, accum = 0;
	
	assert(pt && prevflags);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte = (struct ert_intern *)__pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT), 
						  addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, &accum);
	if (__pgtbl_isnull(pte, 0, 0)) return -ENOENT;

	orig_v = (u32_t)(pte->next);
	/* accum contains flags from pgd as well, so don't use it to
	 * get prevflags. */
	*prevflags = orig_v & PGTBL_FLAG_MASK;
	/* and update the flags. */
	__pgtbl_setleaf(pte, (void *)((orig_v & PGTBL_FRAME_MASK) | ((u32_t)flags & PGTBL_FLAG_MASK)));

	return 0;
}

static int
pgtbl_mapping_del(pgtbl_t pt, u32_t addr)
{
	unsigned long accum = 0, *pte = NULL;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	return __pgtbl_expandn(pt, addr >> PGTBL_PAGEIDX_SHIFT, 
			       PGTBL_DEPTH+1, &accum, &pte, NULL);
}

static paddr_t
pgtbl_lookup(pgtbl_t pt, u32_t addr, u32_t *flags)
{ 
	void * ret = __pgtbl_lkupan((pgtbl_t)((unsigned long)pt | PGTBL_PRESENT), 
				    addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH+1, flags); 
	if (*flags & PGTBL_PRESENT) return (paddr_t)ret;
	else                        return (paddr_t)NULL;
}

/* vaddr -> kaddr */
static vaddr_t
pgtbl_translate(pgtbl_t pt, u32_t addr, u32_t *flags)
{ 
	paddr_t p = pgtbl_lookup(pt, addr, flags);
	if (!p) return (vaddr_t)NULL;
	return (vaddr_t)chal_pa2va((void*)p);
}

static pgtbl_t pgtbl_create(void *page) { return pgtbl_alloc(page); }

static void pgtbl_init(void) { return; }

#endif /* PGTBL_H */
