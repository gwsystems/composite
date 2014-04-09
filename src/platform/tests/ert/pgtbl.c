/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/***
 * This is a test of the ertrie code that essentially emulates the
 * special constraints of a page-table to make sure they work for that
 * specific case.
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

typedef unsigned short int u16_t;
typedef unsigned int u32_t;
typedef unsigned long vaddr_t;
#define unlikely(x)     __builtin_expect(!!(x), 0)
#define LINUX_TEST
#include <ertrie.h>

/* these should change to reflect the hardware layout */
typedef enum {
	PGTBL_PRESENT      = 0,
	PGTBL_WRITABLE     = 1<<0,
	PGTBL_USER         = 1<<1,
	PGTBL_WT           = 1<<2, 	/* write-through caching */
	PGTBL_NOCACHE      = 1<<3, 	/* caching disabled */
	PGTBL_ACCESSED     = 1<<4,
	PGTBL_MODIFIED     = 1<<5,
	PGTBL_SUPER        = 1<<6, 	/* super-page (4MB on x86-32) */
	PGTBL_GLOBAL       = 1<<7,
	PGTBL_COSFRAME     = 1<<8,
	PGTBL_INTERN_DEF   = PGTBL_PRESENT  | PGTBL_WRITABLE | PGTBL_USER | 
	                     PGTBL_ACCESSED | PGTBL_MODIFIED,
};

#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH         2

/* TODO: replace with proper translations */
static inline void *chal_pa2va(void *p) { return (char*)~(unsigned long)p; }
static inline void *chal_va2pa(void *v) { return (char*)~(unsigned long)v; }

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
__pgtbl_get(struct ert_intern *a, unsigned long *accum, int isleaf)
{ 
	(void)isleaf; 
	*accum |= (((unsigned long)a->next) & PGTBL_FLAG_MASK); 
	return chal_pa2va(((unsigned long)a->next) & PGTBL_FRAME_MASK); 
}
static int __pgtbl_isnull(struct ert_intern *a, unsigned long *accum, int isleaf) 
{ (void)isleaf; (void)accum; return !(a->next & (PGTBL_PRESENT|PGTBL_COSFRAME)); }
static void __pgtbl_init(struct ert_intern *a, int isleaf) 
{ 
	if (isleaf) return; 
	a->next = NULL;
}
/* TODO: cas */
static inline void __captbl_setleaf(struct ert_intern *a, void *v)
{ a->next = (void*)(chal_va2pa(v & PGTBL_FRAME_MASK) | (v & PGTBL_FLAG_MASK)); }
static void __pgtbl_set(struct ert_intern *a, void *v, void *accum, int isleaf) 
{ (void)accum; assert(!isleaf); __captbl_setleaf(a, v); }

static inline void *__captbl_getleaf(struct ert_intern *a, void *accum)
{ return __pgtbl_get(a, accum, 1); }

ERT_CREATE(__pgtbl, pgtbl, PGTBL_DEPTH, 10, 10, 4, 4, NULL,		\
	   __pgtbl_init, __pgtbl_get, __pgtbl_isnull, __pgtbl_set, __pgtbl_a, \
	   __pgtbl_setleaf, __pgtbl_getleaf, ert_defresolve);

/* make it an opaque type...not to be touched */
typedef struct pgtbl * pgtbl_t; 

static pgtbl_t pgtbl_alloc(void *page) 
{ return __pgtbl_alloc(&page) & PGTBL_FRAME_MASK; }

static int 
pgtbl_intern_expand(pgtbl_t pt, vaddr_t addr, void *pte, u32_t flags)
{
	unsigned long accum = (unsigned long)flags;
	int ret;

	assert(pt);
	assert((PGTBL_FLAG_MASK & pte) == 0);
	assert((PGTBL_FLAG_MASK & flags) == 0);

	if (!pte) return -EINVAL;
	ret = __pgtbl_expandn(pt, addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, &accum, &pte, NULL);
	if (!ret && pte) return -EEXIST; /* no need to expand */
	assert(!(ret && !pte));		 /* error and used memory??? */

	return ret;
}

static void *
pgtbl_intern_shrink(pgtbl_t pt, vaddr_t addr)
{
	unsigned long accum = 0, *pgd;
	void *page;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	pgd = __pgtbl_lkupan(pt, addr >> PGTBL_PAGEIDX_SHIFT, 1, &accum);
	if (unlikely(!pgd)) return -ENOENT;
	page = __pgtbl_get(pgd, &accum, 0);
	accum = 0;
	__pgtbl_set(pgd, NULL, &accum, 0);

	return 0;
}

static int
pgtbl_mapping_add(pgtbl_t pt, vaddr_t addr, void *page, u32_t flags)
{
	unsigned long accum = 0, *pte = NULL;

	assert(pt);
	assert((PGTBL_FLAG_MASK & pte) == 0);
	assert((PGTBL_FLAG_MASK & flags) == 0);

	return __pgtbl_expandn(pt, addr >> PGTBL_PAGEIDX_SHIFT, 
			       PGTBL_DEPTH+1, &accum, &pte, page | flags);
}

static int
pgtbl_mapping_mod(pgtbl_t pt, vaddr_t addr, u32_t flags, u32_t *prevflags)
{
	unsigned long *pte;
	void *page;
	
	assert(pt && prevflags);
	assert((PGTBL_FLAG_MASK & pte) == 0);
	assert((PGTBL_FLAG_MASK & flags) == 0);

	*prevflags = 0;
	page = __pgtbl_lkupan(pt, addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH+1, prevflags);
	if (!page) return -ENOENT;
	*pte = (unsigned long)__pgtbl_set(page, &(unsigned long)flags, 1);

	return 0;
}

static int
pgtbl_mapping_del(pgtbl_t pt, vaddr_t addr)
{
	unsigned long accum = 0, *pte = NULL;

	return __pgtbl_expandn(pt, addr >> PGTBL_PAGEIDX_SHIFT, 
			       PGTBL_DEPTH+1, &accum, &pte, NULL);
}

/* vaddr -> kaddr */
static void *
pgtbl_translate(pgtbl_t pt, vaddr_t addr, u32_t *flags)
{ return __pgtbl_lkupan(pt, addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH+1, flags); }
