/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef PGTBL_H
#define PGTBL_H

#include "shared/cos_errno.h"
#include "ertrie.h"
#include "chal/util.h"
#include "captbl.h"
#include "retype_tbl.h"
#include "liveness_tbl.h"
#include "chal/defs.h"

#ifndef LINUX_TEST
#include "chal.h"
#endif

/* PRY:MODIFIED THIS FOR CORTEX-M */
typedef enum {
	PGTBL_PRESENT      = 1,
	PGTBL_LEAF         = 1<<1,
	PGTBL_WRITABLE     = 1<<2,
	PGTBL_USER         = 1<<3,
	PGTBL_WT           = 1<<4, 	/* write-through caching */
	PGTBL_NOCACHE      = 1<<5, 	/* caching disabled */
	/* Composite defined bits next*/
	PGTBL_COSFRAME     = 1<<6,
	PGTBL_COSKMEM      = 1<<7,     /* page activated as kernel object */
	/* Flag bits done. */

	PGTBL_USER_DEF     = PGTBL_PRESENT|PGTBL_LEAF|PGTBL_USER|PGTBL_WRITABLE|PGTBL_WT|PGTBL_COSFRAME,
	PGTBL_INTERN_DEF   = PGTBL_PRESENT,
} pgtbl_flags_t;

/* PRY:MODIFIED THIS FOR CORTEX-M */
#define PGTBL_PAGEIDX_SHIFT (24)
#define PGTBL_FRAME_BITS    (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
/* These macros are now useless */
/*
#define PGTBL_DEPTH         2
#define PGTBL_ORD           10
*/


/* The page table implementation for CM3: the page table is always 64 slots, and the mappings are fixed, so as if there is no mapping.
 * The only bit that is useful is the bit "PGTBL_PRESENT". The effective bits are always 6 bits for each frame.
 * Cache:4-way set associative, 32 bytes per line */
/*
#define PGTBL_PAGEIDX_SHIFT (26)
#define PGTBL_FRAME_BITS    (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH         1
#define PGTBL_ORD           64
*/

struct tlb_quiescence {
	/* Updated by timer. */
	u64_t last_periodic_flush;
	/* Updated by tlb flush IPI. */
	u64_t last_mandatory_flush;
	/* cacheline size padding. */
	u8_t __padding[CACHE_LINE - 2 * sizeof(u64_t)];
} __attribute__((aligned(CACHE_LINE), packed)) ;

///*
// * Use the passed in page, but make sure that we only use the passed
// * in page once.
// */
//static inline void *
//__pgtbl_a(void *d, int sz, int leaf)
//{
//	void **i = d, *p;
//
//	(void)leaf;
//	assert(sz == PAGE_SIZE);
//	if (unlikely(!*i)) return NULL;
//	p = *i;
//	*i = NULL;
//	return p;
//}
//static struct ert_intern *
//__pgtbl_get(struct ert_intern *a, void *accum, int isleaf)
//{
//	(void)isleaf;
//	/* don't use | here as we only want the pte flags */
//	*(u32_t*)accum = (((u32_t)a->next) & PGTBL_FLAG_MASK);
//	return chal_pa2va((paddr_t)((((u32_t)a->next) & PGTBL_FRAME_MASK)));
//}
//static int __pgtbl_isnull(struct ert_intern *a, void *accum, int isleaf)
//{ (void)isleaf; (void)accum; return !(((u32_t)(a->next)) & (PGTBL_PRESENT|PGTBL_COSFRAME)); }
//static void
//__pgtbl_init(struct ert_intern *a, int isleaf)
//{
//	(void)isleaf;
////	if (isleaf) return;
//	a->next = NULL;
//}
//
///* We only need to do mapping_add at boot time to add all physical
// * memory to the pgtbl of llboot. After that, we only need to do copy
// * from llboot pgtbl to other pgtbls. Thus, when adding to pgtbl, we
// * use physical addresses; when doing copy, we don't need to worry
// * about PA. */
//
///* v should include the desired flags */
//static inline int
//__pgtbl_setleaf(struct ert_intern *a, void *v)
//{
//	u32_t new, old;
//
//	old = (u32_t)(a->next);
//	new = (u32_t)(v);
//
//	if (!cos_cas((unsigned long *)a, old, new)) return -ECASFAIL;
//
//	return 0;
//}
//
///* This takes an input parameter as the old value of the mapping. Only
// * update when the existing value matches. */
//static inline int
//__pgtbl_update_leaf(struct ert_intern *a, void *v, u32_t old)
//{
//	u32_t new;
//
//	new = (u32_t)(v);
//	if (!cos_cas((unsigned long *)a, old, new)) return -ECASFAIL;
//
//	return 0;
//}
//
///* Note:  We're just using pre-defined default flags for internal (pgd) entries */
//static int
//__pgtbl_set(struct ert_intern *a, void *v, void *accum, int isleaf)
//{
//	u32_t old, new;
//	(void)accum; assert(!isleaf);
//
//	old = (u32_t)a->next;
//	new = (u32_t)chal_va2pa((void*)((u32_t)v & PGTBL_FRAME_MASK)) | PGTBL_INTERN_DEF;
//
//	if (!cos_cas((unsigned long *)&a->next, old, new)) return -ECASFAIL;
//
//	return 0;
//}
//
//static inline void *__pgtbl_getleaf(struct ert_intern *a, void *accum)
//{ if (unlikely(!a)) return NULL; return __pgtbl_get(a, accum, 1); }
/*
ERT_CREATE(__pgtbl, pgtbl, PGTBL_DEPTH, PGTBL_ORD, sizeof(int*), PGTBL_ORD, sizeof(int*), NULL, \
	   __pgtbl_init, __pgtbl_get, __pgtbl_isnull, __pgtbl_set,	\
	   __pgtbl_a, __pgtbl_setleaf, __pgtbl_getleaf, ert_defresolve);
*/
/* How many bits have been translated ? */
/*
first layer, size_order 2^32, 5 bits to represent. However we only allow 8-pgtbls, thus, the last layer is fixed to 8.

entry_order? ignore, because entry_order is really sucky.
If the type is MPUMETA, then the type_addr will be the address of the next pgtbl structure.
what about the refcnt? add refcnt when being referenced?
we do not check refcnts? no.
start addr, size order, entry_order. minimal page is 256B or etc. 2<<16, even 8
size - minimal is 128 byte pages. -
128 256 512 1k 2k 4k 8k 16k 32k 64k 128k 256k 512k 1M 2M 4M
 0   1   2  3  4  5  6   7   8   9   10   11   12  13 14 15

 2   4   8  16 32 64 128 256
 0   1   2  3  4  5   6   7

The size of a page can be 2^16=
start addr[31:8] size_order[7:4] num_order[3:1] Type[0]
For leaf nodes:
start_addr[31:8] status[7:0]
For internal nodes:
ptr [31:2] flag[1:0]
*/
#define COS_PGTBL_MPUMETA          0x00
#define COS_PGTBL_TABLE            0x01

#define COS_PGTBL_METAADDR(X)      ((X)&(~(0x03)))

#define COS_PGTBL_STARTADDR(X)     ((X)&(0xFFFFFF00))
#define COS_PGTBL_SIZEORDER(X)     ((((X)&0xFF)>>3)+7)
#define COS_PGTBL_NUMORDER(X)      ((((X)&0x0E)>>1)+1)
#define COS_PGTBL_TYPE(X)          ((X)&0x01)

#define COS_PGTBL_TYPEADDR(ADDR,SIZE_ORDER,NUM_ORDER,TYPE)    (((ADDR)&(~0xFF))|((SIZE_ORDER-7)<<3)|((NUM_ORDER-1)<<1)|(TYPE))
#define COS_PGTBL_CHILD(ADDR,FLAGS)                           ((((u32_t)(ADDR))&(~0x03))|(FLAGS))
#define COS_PGTBL_LEAF(ADDR,FLAGS)                            (((ADDR)&(~0xFF))|(FLAGS))

#define COS_PGTBL_SIZE_128B    7
#define COS_PGTBL_SIZE_256B    8
#define COS_PGTBL_SIZE_512B    9
#define COS_PGTBL_SIZE_1K      10
#define COS_PGTBL_SIZE_2K      11
#define COS_PGTBL_SIZE_4K      12
#define COS_PGTBL_SIZE_8K      13
#define COS_PGTBL_SIZE_16K     14
#define COS_PGTBL_SIZE_32K     15
#define COS_PGTBL_SIZE_64K     16
#define COS_PGTBL_SIZE_128K    17
#define COS_PGTBL_SIZE_256K    18
#define COS_PGTBL_SIZE_512K    19
#define COS_PGTBL_SIZE_1M      20
#define COS_PGTBL_SIZE_2M      21
#define COS_PGTBL_SIZE_4M      22

#define COS_PGTBL_NUM_2        1
#define COS_PGTBL_NUM_4        2
#define COS_PGTBL_NUM_8        3
#define COS_PGTBL_NUM_16       4

/*
union pgtbl_data
{
	struct MPU;
	struct pgtbl;
};
*/
struct pgtbl  /* 9 words */
{
	u32_t type_addr;
	/* For MPU there are 5(4) regions' space, for pgtbl there are 8 pages' space. The start address is stored in the child nodes.
	 * We no longer allocate memory for page tables. we always use 8 pages. data[9] will contain the top-level addr, if needed */
	u32_t data[10];
};

/* identical to the capability structure */
struct cap_pgtbl {     /* 16 words */
	struct cap_header h;                                              /* 1 */
	u32_t refcnt_flags;          /* includes refcnt and flags */      /* 1 */
	struct pgtbl pgtbl;                                               /* 11 */
	struct cap_pgtbl *parent;    /* if !null, points to parent cap */ /* 1 */
	u64_t frozen_ts;             /* timestamp when frozen is set. */  /* 2 */
} __attribute__((packed));

/* make it an opaque type...not to be touched */
/* PRY:We change pgtbl_t here. How do we inline these ? */
/* inline the MPU representation into the pgtbl caps:MPU needs, say, 4 slots, which is 8 words */
typedef struct pgtbl * pgtbl_t;

///* identical to the capability structure */
//struct cap_pgtbl {     /* 7 words */
//	struct cap_header h;                                              /* 1 */
//	u32_t refcnt_flags;          /* includes refcnt and flags */      /* 1 */
//	pgtbl_t pgtbl;                                                    /* 1 */
//	u32_t lvl; 		     /* what level are the pgtbl nodes at? */     /* 1 */
//	struct cap_pgtbl *parent;    /* if !null, points to parent cap */ /* 1 */
//	u64_t frozen_ts;             /* timestamp when frozen is set. */  /* 2 */
//} __attribute__((packed));
/* PRY:the page table lookup function - look up for a pte, its size order, and flags */
static u32_t* pgtbl_lkup_pte(pgtbl_t pt, vaddr_t addr, u32_t* size_order, u32_t* flags)
{
	u32_t start_addr;
	u32_t excess_addr;
	u32_t sz_order;

	/* The pgtbl_t is empty */
	if(pt==0)
		return 0;

	/* what is this pt? If this one contains MPU metadata, then we look for its next level */
	if(COS_PGTBL_TYPE(pt->type_addr)==COS_PGTBL_MPUMETA)
		pt=COS_PGTBL_METAADDR(pt->type_addr);

	while(1)
	{
		/* Get the start address of this */
		start_addr=COS_PGTBL_STARTADDR(pt->type_addr);
		/* Start translation */
		excess_addr=addr-start_addr;
		sz_order=COS_PGTBL_SIZEORDER(pt->type_addr);
		/* Not in range */
		if((excess_addr>>sz_order)>8)
			return 0;

		/* See if this entry exists */
		if((pt->data[excess_addr>>sz_order]&PGTBL_PRESENT)==0)
			return 0;

		if((pt->data[excess_addr>>sz_order]&PGTBL_COSFRAME)==0)
			return 0;

		/* See what is in this entry. Is this a pte or an internal node */
		if((pt->data[excess_addr>>sz_order]&PGTBL_LEAF)!=0)
			break;

		/* Must be an internal node */
		pt=COS_PGTBL_STARTADDR(pt->type_addr);
	}
	*size_order=sz_order;
	return &(pt->data[excess_addr>>sz_order]);
}

static u32_t _pgtbl_isnull(u32_t pte)
{
	if(pte==0)
		return 1;
}
//static pgtbl_t pgtbl_alloc(void *page)
//{ return (pgtbl_t)((unsigned long)__pgtbl_alloc(&page) & PGTBL_FRAME_MASK); }
//
//static void
//pgtbl_init_pte(void *pte)
//{
//	int i;
//	unsigned long *vals = pte;
//
//	for (i = 0 ; i < (1<<PGTBL_ORD) ; i++) vals[i] = 0;
//}
//
//static int
//pgtbl_intern_expand(pgtbl_t pt, u32_t addr, void *pte, u32_t flags)
//{
//	unsigned long accum = (unsigned long)flags;
//	int ret;
//
//	/* NOTE: flags currently ignored. */
//
//	assert(pt);
//	assert((PGTBL_FLAG_MASK & (u32_t)pte) == 0);
//	assert((PGTBL_FRAME_MASK & flags) == 0);
//
//	if (!pte) return -EINVAL;
//	ret = __pgtbl_expandn(pt, (unsigned long)(addr >> PGTBL_PAGEIDX_SHIFT),
//			      PGTBL_DEPTH, &accum, &pte, NULL);
//	if (!ret && pte) return -EEXIST; /* no need to expand */
//	assert(!(ret && !pte));		 /* error and used memory??? */
//
//	return ret;
//}
//
///*
// * FIXME: If these need to return a physical address, we should do a
// * va2pa before returning
// */
//static void *
//pgtbl_intern_prune(pgtbl_t pt, u32_t addr)
//{
//	unsigned long accum = 0, *pgd;
//	void *page;
//
//	assert(pt);
//	assert((PGTBL_FLAG_MASK & (u32_t)addr) == 0);
//
//	pgd = __pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT), (u32_t)addr >> PGTBL_PAGEIDX_SHIFT, 1, &accum);
//	if (!pgd) return NULL;
//	page = __pgtbl_get((struct ert_intern *)pgd, &accum, 0);
//	accum = 0;
//
//	if (__pgtbl_set((struct ert_intern *)pgd, NULL, &accum, 0)) return NULL;
//
//	return page;
//}
//
///* FIXME:  these pgd functions should be replaced with lookup_lvl functions (see below) */
//static void *
//pgtbl_get_pgd(pgtbl_t pt, u32_t addr)
//{
//	unsigned long accum = 0;
//
//	assert(pt);
//	return __pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT), (u32_t)addr >> PGTBL_PAGEIDX_SHIFT, 1, &accum);
//}
//
//static int
//pgtbl_check_pgd_absent(pgtbl_t pt, u32_t addr)
//{ return __pgtbl_isnull(pgtbl_get_pgd(pt, (u32_t)addr), 0, 0); }
//
//extern struct tlb_quiescence tlb_quiescence[NUM_CPU] CACHE_ALIGNED;
//
//int tlb_quiescence_check(u64_t timestamp);
//
//
static inline int
pgtbl_quie_check(u32_t orig_v)
{
	return 0;
}

/*
 * this works on both kmem and regular user memory: the retypetbl_ref
 * works on both. addr is the address to map at; page is the physical address/
 */
static int //__attribute__((optimize("O0")))
pgtbl_mapping_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	u32_t size_order;
	u32_t old_flags;
	u32_t* pte;
	int ret = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* Get the pte */
	pte=pgtbl_lkup_pte(pt, addr, &size_order,&old_flags);

	if (!pte) return -ENOENT;

	if (*pte & PGTBL_PRESENT)  return -EEXIST;
	if (*pte & PGTBL_COSFRAME) return -EPERM;

	/* ref cnt on the frame - we assume that this will succeed */
	ret = retypetbl_ref((void *)page);
	if (ret) return ret;

	*pte|=flags;
	/*
	ret = __pgtbl_update_leaf(pte, (void *)(page | flags), orig_v);
	*/
	/* PRY:we just assume that all operations are successful */
	/* restore the refcnt if necessary */
	/*
	if (ret) retypetbl_deref((void *)page);
     */

	return ret;
}

//static int //__attribute__((optimize("O0")))
//pgtbl_mapping_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
//{
//	int ret = 0;
//	struct ert_intern *pte;
//	u32_t orig_v, accum = 0;
//
//	assert(pt);
//	assert((PGTBL_FLAG_MASK & page) == 0);
//	assert((PGTBL_FRAME_MASK & flags) == 0);
//
//	/* get the pte */
//	pte = (struct ert_intern *)__pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT),
//						  addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, &accum);
//	if (!pte) return -ENOENT;
//	orig_v = (u32_t)(pte->next);
//
//	if (orig_v & PGTBL_PRESENT)  return -EEXIST;
//	if (orig_v & PGTBL_COSFRAME) return -EPERM;
//
//	/* Quiescence check */
//	ret = pgtbl_quie_check(orig_v);
//	if (ret) return ret;
//
//	/* ref cnt on the frame. */
//	ret = retypetbl_ref((void *)page);
//	if (ret) return ret;
//
//	ret = __pgtbl_update_leaf(pte, (void *)(page | flags), orig_v);
//	/* restore the refcnt if necessary */
//	if (ret) retypetbl_deref((void *)page);
//
//	return ret;
//}

/*
 * FIXME: a hack used to get more kmem available in Linux booting
 * environment. Only used when booting up in Linux (hijack.c). This
 * just adds the Linux allocated kmem into pgtbl w/o checking
 * quiescence or refcnt.
 */
static int
kmem_add_hack(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	int ret;
	u32_t* pte;
	u32_t old_flags;
	u32_t size_order;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte=_pgtbl_lookup_pte(pt, addr, &size_order, &old_flags);

	if (*pte & PGTBL_PRESENT)  return -EEXIST;
	if (*pte & PGTBL_COSFRAME) return -EPERM;

	/* We assume that the update is always successful */
	*pte|=flags;
	ret = 0;/*__pgtbl_update_leaf(pte, (void *)(page | flags), orig_v); */

	return ret;
}

//static int
//kmem_add_hack(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
//{
//	int ret;
//	struct ert_intern *pte;
//	u32_t orig_v, accum = 0;
//
//	assert(pt);
//	assert((PGTBL_FLAG_MASK & page) == 0);
//	assert((PGTBL_FRAME_MASK & flags) == 0);
//
//	/* get the pte */
//	pte = (struct ert_intern *)__pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT),
//						  addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, &accum);
//	orig_v = (u32_t)(pte->next);
//
//	if (orig_v & PGTBL_PRESENT)  return -EEXIST;
//	if (orig_v & PGTBL_COSFRAME) return -EPERM;
//
//	ret = __pgtbl_update_leaf(pte, (void *)(page | flags), orig_v);
//
//	return ret;
//}

/* This function is only used by the booting code to add cos frames to
 * the pgtbl. It ignores the retype tbl (as we are adding untyped
 * frames). */
static int
pgtbl_cosframe_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	u32_t* pte;
	u32_t size_order;
	u32_t orig_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte=_pgtbl_lookup(pt, addr, &size_order);
	assert (*pte == 0);

	/* Assume this operation is always successful */
	*pte|=flags;

	return 0;/* __pgtbl_update_leaf(pte, (void *)(page | flags), 0); */
}

/* This function updates flags of an existing mapping. */
static int
pgtbl_mapping_mod(pgtbl_t pt, u32_t addr, u32_t flags, u32_t *prevflags)
{
    /* Not used for now. TODO: add retypetbl_ref / _deref */
	u32_t *pte;
	u32_t size_order;
	u32_t orig_v, accum = 0;

	assert(pt && prevflags);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte=_pgtbl_lookup(pt, addr, &size_order);
	if (!(*pte & PGTBL_PRESENT))  return -ENOENT;
	/*
	 * accum contains flags from pgd as well, so don't use it to
	 * get prevflags.
	 */
	*prevflags = *pte & PGTBL_FLAG_MASK;
	/* We assume that this is always successful */
	*pte|=flags;
	/* and update the flags. */
	return 0;/*__pgtbl_update_leaf(pte, (void *)((orig_v & PGTBL_FRAME_MASK) | ((u32_t)flags & PGTBL_FLAG_MASK)), orig_v); */
}

/* When we remove a mapping, we need to link the vas to a liv_id,
 * which tracks quiescence for us. */
static int
pgtbl_mapping_del(pgtbl_t pt, u32_t addr, u32_t liv_id)
{
	int ret;
	u32_t *pte;
	u32_t old_flags;
	u32_t size_order;

	unsigned long orig_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* In pgtbl, we have only 20bits for liv id. */
	if (unlikely(liv_id >= (1 << (32-PGTBL_PAGEIDX_SHIFT)))) return -EINVAL;

	/* Liveness tracking of the unmapping VAS. */
	ret = ltbl_timestamp_update(liv_id);
	if (unlikely(ret)) goto done;

	/* get the pte */
	pte=pgtbl_lkup_pte(pt, addr, &size_order,&old_flags);
	if (!(*pte & PGTBL_PRESENT)) return -EEXIST;
	if (*pte & PGTBL_COSFRAME)   return -EPERM;

	/* We assume that this is always successful */
	*pte|=(liv_id<<PGTBL_PAGEIDX_SHIFT);

	/* decrement ref cnt on the frame. */
	ret = retypetbl_deref((void *)(*pte & PGTBL_FRAME_MASK));
	if (ret) cos_throw(done, ret);

done:
	return ret;
}

/* NOTE: This just removes the mapping. NO liveness tracking! TLB
 * flush should be taken care of separately (and carefully). */
static int
pgtbl_mapping_del_direct(pgtbl_t pt, u32_t addr)
{
	unsigned long accum = 0;
	u32_t* pte = NULL;
	u32_t size_order;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* get the pte */
	pte=_pgtbl_lookup(pt, addr, &size_order);
	*pte=0;

	return 0;
}

//static void *pgtbl_lkup_lvl(pgtbl_t pt, u32_t addr, u32_t *flags, u32_t start_lvl, u32_t end_lvl)
//{ return __pgtbl_lkupani((pgtbl_t)((unsigned long)pt | PGTBL_PRESENT),
//			 addr >> PGTBL_PAGEIDX_SHIFT, start_lvl, end_lvl, flags); }

static int pgtbl_ispresent(u32_t flags)
{ return flags & (PGTBL_PRESENT|PGTBL_COSFRAME); }

//static unsigned long *
//pgtbl_lkup(pgtbl_t pt, u32_t addr, u32_t *flags)
//{
//	void *ret;
//
//	ret = __pgtbl_lkupan((pgtbl_t)((unsigned long)pt | PGTBL_PRESENT),
//			     addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH+1, flags);
//	if (!pgtbl_ispresent(*flags)) return NULL;
//	return ret;
//}
//
///* Return the pointer of the pte.  */
//static unsigned long *
//pgtbl_lkup_pte(pgtbl_t pt, u32_t addr, u32_t *flags)
//{
//	return __pgtbl_lkupan((pgtbl_t)((unsigned long)pt | PGTBL_PRESENT),
//			      addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, flags);
//}
//
///* FIXME: remove this function.  Why do we need a paddr lookup??? */
//static paddr_t
//pgtbl_lookup(pgtbl_t pt, u32_t addr, u32_t *flags)
//{
//	unsigned long *ret = pgtbl_lkup(pt, addr, flags);
//	if (!ret) return (paddr_t)NULL;
//	return (paddr_t)chal_va2pa(ret);
//}

static int
pgtbl_get_cosframe(pgtbl_t pt, vaddr_t frame_addr, paddr_t *cosframe)
{
	u32_t size_order;
	u32_t old_flags;
	unsigned long *pte;
	paddr_t v;

	pte = pgtbl_lkup_pte(pt, frame_addr, &size_order, &old_flags);
	if (!pte) return -EINVAL;

	v = *pte;
	if (!(v & PGTBL_COSFRAME)) return -EINVAL;

	*cosframe = v & PGTBL_FRAME_MASK;

	return 0;
}
/* PRY: no such thing
extern unsigned long __cr3_contents;
*/
// this helps debugging.
// #define UPDATE_LINUX_MM_STRUCT

/* If Composite is running at the highest priority, then we don't need
 * to touch the mm_struct. Also, don't set this when we want return to
 * Linux on idle.*/
#ifndef LINUX_HIGHEST_PRIORITY
#undef UPDATE_LINUX_MM_STRUCT
#define UPDATE_LINUX_MM_STRUCT
#endif
#ifdef LINUX_ON_IDLE
#undef UPDATE_LINUX_MM_STRUCT
#define UPDATE_LINUX_MM_STRUCT
#endif

static inline void
pgtbl_update(pgtbl_t pt)
{
	/* PRY:This page table must be the top-level page table that contains the MPU data */
	/* Do MPU data loading here */
	/*asm volatile("mov %0, %%cr3" : : "r"(pt));*/
}

/* vaddr -> kaddr */
static vaddr_t
pgtbl_translate(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	u32_t* pte;
	u32_t size_order;
	pte=_pgtbl_lookup(pt, addr, &size_order);
	if(pte==0)
		return 0;

	/* If the pte is there, we return it */
	if(*pte!=0)
		return COS_PGTBL_METAADDR(*pte);
	else
		return 0;
}

/* FIXME: this should be using cos_config.h defines */
#define KERNEL_PGD_REGION_OFFSET  (PAGE_SIZE - PAGE_SIZE/4)
#define KERNEL_PGD_REGION_SIZE    (PAGE_SIZE/4)

/* PRY:this function no longer called because all nodes are embedded */
//static pgtbl_t pgtbl_create(void *page, void *curr_pgtbl) {
//	pgtbl_t ret = pgtbl_alloc(page);
//	/* Copying the kernel part of the pgd. */
//	memcpy(page + KERNEL_PGD_REGION_OFFSET, (void *)chal_pa2va((paddr_t)curr_pgtbl) + KERNEL_PGD_REGION_OFFSET, KERNEL_PGD_REGION_SIZE);
//
//	return ret;
//}
/* PRY: modified the function definitions here */
int //__attribute__((optimize("O0")))
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, u32_t type, u32_t start_addr, u32_t size_order, u32_t num_order);
/* int pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl); */
int pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin,
		     livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr, const int root);

static int
pgtbl_mapping_scan(struct cap_pgtbl *pt)
{
//	unsigned int i, pte, *page;
//	livenessid_t lid;
//	u64_t past_ts;
//
//	/* This scans the leaf level of the pgtbl and verifies
//	 * quiescence. - we are always quiescent, so return 0 */
//	if (pt->lvl != PGTBL_DEPTH - 1) return -EINVAL;
//
//	page = (unsigned int *)(pt->pgtbl);
//	assert(page);
//
//	for (i = 0; i < PAGE_SIZE / sizeof(int *); i++) {
//		pte = *(page + i);
//		if (pte & PGTBL_PRESENT || pte & PGTBL_COSFRAME) return -EINVAL;
//
//		if (pte & PGTBL_QUIESCENCE) {
//			lid = pte >> PGTBL_PAGEIDX_SHIFT;
//
//			if (ltbl_get_timestamp(lid, &past_ts)) return -EFAULT;
//			if (!tlb_quiescence_check(past_ts)) return -EQUIESCENCE;
//		}
//	}

	return 0;
}

static void pgtbl_init(void) {
	assert(sizeof(struct cap_pgtbl) <= __captbl_cap2bytes(CAP_PGTBL));

	return;
}

int cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr);
int pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte);

#endif /* PGTBL_H */
