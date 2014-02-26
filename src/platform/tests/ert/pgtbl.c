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
	PGTBL_PRESENT  = 1,
	PGTBL_WRITE    = 1<<1,
	PGTBL_ACCESSED = 1<<2,
	PGTBL_MODIFIED = 1<<3,
	PGTBL_KERNEL   = 1<<4,
	PGTBL_GLOBAL   = 1<<5,
	PGTBL_SUPER    = 1<<6,
};
#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_PAGE_MASK     (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH 2

static inline void *chal_pa2va(void *p) { return ((char*)p + (4096*4096)); }
static inline void *chal_va2pa(void *v) { return ((char*)v - (4096*4096)); }

/* 
 * These allocation functions should actually use the data pointer to
 * derive how to allocate the memory.
 */
#include <sys/mman.h>
struct mem_info {
	void *page;
};
static inline void *
__pgtbl_a(void *d, int sz, int leaf) 
{ 
	struct mem_info *i = d;
	void *p;

	(void)d; (void)leaf;
	assert(sz == 4096);
	if (unlikely(!i->page)) return NULL;
	p = i->page;
	i->page = NULL;
	return p;
//	return mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}
static struct ert_intern *
__pgtbl_get(struct ert_intern *a, unsigned long *accum, int isleaf)
{ (void)accum; (void)isleaf; *accum |= (a & PGTBL_FLAG_MASK); return chal_pa2va(a & PGTBL_PAGE_MASK); }
static int 
__pgtbl_isnull(struct ert_intern *a, unsigned long *accum, int isleaf) 
{ (void)isleaf; (void)accum; return !(a & PGTBL_PRESENT); }
static void *__pgtbl_init(struct ert_intern *a, int isleaf) { (void)a; (void)isleaf; return NULL; }
static void *__pgtbl_set(void *v, unsigned long *a, int isleaf) { (void)isleaf; return chal_va2pa(v) | *a; }

ERT_CREATE(__pgtbl, 2, 10, 10, 4, NULL, __pgtbl_init, __pgtbl_get, __pgtbl_isnull, __pgtbl_set, __pgtbl_a);
/* make it an opaque type...not to be touched */
typedef struct __pgtbl_ert *pgtbl_t; 

static pgtbl_t
pgtbl_alloc(void *page)
{
	unsigned long accum;
	struct mem_info mi;
	
	mi->page = page;
	return __pgtbl_alloc(&mi);
}

static int 
pgtbl_intern_expand(pgtbl_t pt, vaddr_t addr, int max_lvl, void *pte, u32_t flags)
{
	unsigned long accum = (unsigned long)flags;
	struct mem_info mi;

	assert(lvl > 1 && lvl <= PGTBL_DEPTH);
	assert(pte == PGTBL_PAGE_MASK & pte);
	assert(flags == PGTBL_FLAG_MASK & flags);
	mi->page = pte;

	/* return an error if there was one in expanding, of it we didn't use the page */
	return __pgtbl_expandn(pt, addr >> PGTBL_PAGEIDX_SHIFT, 
			       max_lvl, &accum, &mi) || mi->page != NULL;
}

static int
pgtbl_mapping_add(pgtbl_t pt, vaddr_t addr, void *page, u32_t flags)
{
	unsigned long accum, *pte;
	
	assert(page == PGTBL_PAGE_MASK & page);
	assert(flags == PGTBL_FLAG_MASK & flags);
	pte = __pgtbl_lkupa(pt, addr >> PGTBL_PAGEIDX_SHIFT, &accum);
	if (unlikely(!pte)) return -1; /* no pte */
	*pte = (unsigned long)__pgtbl_set(page, &(unsigned long)flags, 1);

	return 0;
}

static int
pgtbl_mapping_mod(pgtbl_t pt, vaddr_t addr, u32_t flags)
{
	unsigned long accum = 0, *pte;
	void *page;
	
	assert(flags == PGTBL_FLAG_MASK & flags);
	pte = __pgtbl_lkupa(pt, addr >> PGTBL_PAGEIDX_SHIFT, &accum);
	if (unlikely(!pte)) return -1; /* no pte */
	page = __pgtbl_get(pte, &accum, 1);
	*pte = (unsigned long)__pgtbl_set(page, &(unsigned long)flags, 1);

	return 0;
}

/* vaddr -> kaddr */
static void *
pgtbl_translate(pgtbl_t pt, vaddr_t addr, u32_t *flags)
{
	unsigned long accum = 0;
	void *page;

	pte = __pgtbl_lkupa(pt, addr >> PGTBL_PAGEIDX_SHIFT, &accum);
	if (unlikely(!pte)) return -1; /* no pte */
	*pte = (unsigned long)__pgtbl_get(pte, &accum, &(unsigned long)flags, 1);
	return 0;
}
