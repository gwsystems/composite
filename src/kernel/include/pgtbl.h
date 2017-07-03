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

/* Cortex-M Cache:4-way set associative, 32 bytes per line */
#define PGTBL_PAGEIDX_SHIFT (8)
#define PGTBL_FRAME_BITS    (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK     ((1<<PGTBL_PAGEIDX_SHIFT)-1)
#define PGTBL_FRAME_MASK    (~PGTBL_FLAG_MASK)
/* These macros are now useless */
/*
#define PGTBL_DEPTH         2
#define PGTBL_ORD           10
*/

struct tlb_quiescence {
	/* Updated by timer. */
	u64_t last_periodic_flush;
	/* Updated by tlb flush IPI. */
	u64_t last_mandatory_flush;
	/* cacheline size padding. */
	u8_t __padding[CACHE_LINE - 2 * sizeof(u64_t)];
} __attribute__((aligned(CACHE_LINE), packed)) ;

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
start addr[31:8] size_order[7:3] num_order[2:1] Type[0]
For internal nodes:
ptr [31:2] flag[1:0]
*/
#define COS_PGTBL_MPUMETA          0x00
#define COS_PGTBL_INTERN           0x01

#define COS_PGTBL_METAADDR(X)      ((X)&(~(0x03)))

#define COS_PGTBL_NODE_ADDR(X)     ((X)&(0xFFFFFF00))
#define COS_PGTBL_SIZEORDER(X)     ((((X)&0xFF)>>3)+7)
#define COS_PGTBL_NUMORDER(X)      ((((X)&0x07)>>1)+1)
#define COS_PGTBL_TYPE(X)          ((X)&0x01)

#define COS_PGTBL_TYPEADDR(addr,size_order,num_order,type)    (((addr)&(~0xFF))|((size_order-7)<<3)|((num_order-1)<<1)|(type))
#define COS_PGTBL_CHILD(addr,flags)                           ((((u32_t)(addr))&(~0x03))|(flags))
#define COS_PGTBL_LEAF(addr,flags)                            (((addr)&(~0xFF))|(flags))

#define COS_PGTBL_PGSZ_128B    7
#define COS_PGTBL_PGSZ_256B    8
#define COS_PGTBL_PGSZ_512B    9
#define COS_PGTBL_PGSZ_1K      10
#define COS_PGTBL_PGSZ_2K      11
#define COS_PGTBL_PGSZ_4K      12
#define COS_PGTBL_PGSZ_8K      13
#define COS_PGTBL_PGSZ_16K     14
#define COS_PGTBL_PGSZ_32K     15
#define COS_PGTBL_PGSZ_64K     16
#define COS_PGTBL_PGSZ_128K    17
#define COS_PGTBL_PGSZ_256K    18
#define COS_PGTBL_PGSZ_512K    19
#define COS_PGTBL_PGSZ_1M      20
#define COS_PGTBL_PGSZ_2M      21
#define COS_PGTBL_PGSZ_4M      22

#define COS_PGTBL_PGNUM_2        1
#define COS_PGTBL_PGNUM_4        2
#define COS_PGTBL_PGNUM_8        3

struct mpu_data {
	/* Region base address register */
	u32_t rbar;
	/* Region attribute and subregion register */
	u32_t rasr;
};

/* For MPU metadata nodes, the [9] contains the region information. For page table
 * nodes, the [9] contains the page table parent information.
 */
union pgtbl_data {
	struct mpu_data mpu[5];
	u32_t pgt[10];
};

struct pgtbl {  /* 9 words */
	u32_t type_addr;
	/* For MPU there are 5(4) regions' space, for pgtbl there are 8 pages' space. The start address is stored in the child nodes.
	 * We no longer allocate memory for page tables. we always use 8 pages. data[9] will contain the top-level addr, if needed */
	union pgtbl_data data;
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

/* PRY:the page table lookup function - look up for a leaf, its size order, and flags */
static u32_t*
pgtbl_lkup_leaf(pgtbl_t pt, vaddr_t addr, u32_t* size_order, u32_t* flags)
{
	u32_t start_addr;
	u32_t excess_addr;
	u32_t sz_order;

	/* The pgtbl_t is empty */
	if(pt==0)
		return 0;

	/* what is this pt? If this one contains MPU metadata, then we look for its next level */
	if(COS_PGTBL_TYPE(pt->type_addr)==COS_PGTBL_MPUMETA)
		pt=(pgtbl_t)COS_PGTBL_METAADDR(pt->type_addr);

	while(1) {
		/* Get the start address of this */
		start_addr=COS_PGTBL_NODE_ADDR(pt->type_addr);
		/* Start translation */
		excess_addr=addr-start_addr;
		sz_order=COS_PGTBL_SIZEORDER(pt->type_addr);
		/* Not in range */
		if((excess_addr>>sz_order)>8)
			return 0;

		/* See if this entry exists */
		if((pt->data.pgt[excess_addr>>sz_order]&PGTBL_PRESENT)==0)
			return 0;

		if((pt->data.pgt[excess_addr>>sz_order]&PGTBL_COSFRAME)==0)
			return 0;

		/* See what is in this entry. Is this a pte or an internal node */
		if((pt->data.pgt[excess_addr>>sz_order]&PGTBL_LEAF)!=0)
			break;

		/* Must be an internal node */
		pt=(pgtbl_t)COS_PGTBL_NODE_ADDR(pt->type_addr);
	}
	*size_order=sz_order;
	return &(pt->data.pgt[excess_addr>>sz_order]);
}

static u32_t
_pgtbl_isnull(u32_t pte)
{
	if(pte==0)
		return 1;
}

static inline int
pgtbl_quie_check(u32_t orig_v)
{
	return 0;
}

/*
 * this works on both kmem and regular user memory: the retypetbl_ref
 * works on both. addr is the address to map at; page is the physical address/
 */
static int
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
	pte=pgtbl_lkup_leaf(pt, addr, &size_order,&old_flags);

	if (!pte) return -ENOENT;

	if (*pte & PGTBL_PRESENT)  return -EEXIST;
	if (*pte & PGTBL_COSFRAME) return -EPERM;

	/* ref cnt on the frame - we assume that this will succeed */
	ret = retypetbl_ref((void *)page);
	if (ret) return ret;

	*pte|=flags;
	/* PRY:Assume that all operations are successful */
	return ret;
}

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
	pte=(u32_t*)pgtbl_lkup_leaf(pt, addr, &size_order, &old_flags);

	if (*pte & PGTBL_PRESENT)  return -EEXIST;
	if (*pte & PGTBL_COSFRAME) return -EPERM;

	/* We assume that the update is always successful */
	*pte|=flags;
	ret = 0;/*__pgtbl_update_leaf(pte, (void *)(page | flags), orig_v); */

	return ret;
}

/* This function is only used by the booting code to add cos frames to
 * the pgtbl. It ignores the retype tbl (as we are adding untyped
 * frames). */
static int
pgtbl_cosframe_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags)
{
	u32_t* pte;
	u32_t size_order;
	u32_t orig_v, accum = 0;
	u32_t old_flags;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte=(u32_t*)pgtbl_lkup_leaf(pt, addr, &size_order, &old_flags);
	assert (*pte == 0);

	/* Assume this operation is always successful */
	*pte|=flags;

	return 0;
}

/* This function updates flags of an existing mapping. */
static int
pgtbl_mapping_mod(pgtbl_t pt, u32_t addr, u32_t flags, u32_t *prevflags)
{
	/* Not used for now. TODO: add retypetbl_ref / _deref */
	u32_t *pte;
	u32_t size_order;
	u32_t orig_v, accum = 0;
	u32_t old_flags;

	assert(pt && prevflags);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte = (u32_t*)pgtbl_lkup_leaf(pt, addr, &size_order, &old_flags);
	if (!(*pte & PGTBL_PRESENT))  return -ENOENT;
	/*
	 * accum contains flags from pgd as well, so don't use it to
	 * get prevflags.
	 */
	*prevflags = *pte & PGTBL_FLAG_MASK;
	/* See if there is a flag conflict */
	if((flags|(*prevflags))!=*prevflags)
		return -EINVAL;
	/* update the flags. */
	*pte |= flags;
	return 0;
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
	pte = pgtbl_lkup_leaf(pt, addr, &size_order,&old_flags);
	if (!(*pte & PGTBL_PRESENT)) return -EEXIST;
	if (*pte & PGTBL_COSFRAME)   return -EPERM;

	/* Delete the mapping */
	*pte |= (liv_id<<PGTBL_PAGEIDX_SHIFT);

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
	u32_t old_flags;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* get the pte */
	pte=(u32_t*)pgtbl_lkup_leaf(pt, addr, &size_order, &old_flags);
	*pte=0;

	return 0;
}

static int
pgtbl_ispresent(u32_t flags)
{ return flags & (PGTBL_PRESENT|PGTBL_COSFRAME); }

static int
pgtbl_get_cosframe(pgtbl_t pt, vaddr_t frame_addr, paddr_t *cosframe)
{
	u32_t size_order;
	u32_t old_flags;
	u32_t *pte;
	paddr_t v;

	pte =(u32_t*) pgtbl_lkup_leaf(pt, frame_addr, &size_order, &old_flags);
	if (!pte) return -EINVAL;

	v = *pte;
	if (!(v & PGTBL_COSFRAME)) return -EINVAL;

	*cosframe = v & PGTBL_FRAME_MASK;

	return 0;
}

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
	/* This must be the top-level MPU data */
	/* PRY:This page table must be the top-level page table that contains the MPU data */
	/* Do MPU data loading here */
	__asm__ __volatile__(".syntax unified \n\t" \
			     "ldr r0,%[_pgtbl] \n\t" \
			     "add r0, #0x04 \n\t" \
			     "ldm r0!,{r1-r8}; \n\t" \
			     "ldr r0,=0xE000ED9C \n\t" \
			     "stm r0!,{r1-r8}; \n\t" \
			     "dsb \n\t" \
			     : \
			     : [_pgtbl]"m"(pt) \
			     : "r0","r1","r2","r3","r4","r5","r6","r7","r8","memory", "cc");
}

/* vaddr -> kaddr, PRY:guess that we are returning flags as well? */
static vaddr_t
pgtbl_translate(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	u32_t* pte;
	u32_t size_order;

	pte =(u32_t*) pgtbl_lkup_leaf(pt, addr, &size_order, flags);
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

/* PRY: modified the function definitions here */
int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, u32_t type, u32_t start_addr, u32_t size_order, u32_t num_order);
int pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin,
		     livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr, const int root);

static int
pgtbl_mapping_scan(struct cap_pgtbl *pt)
{
	/* We return directly */
	return 0;
}

static void
pgtbl_init(void)
{
	assert(sizeof(struct cap_pgtbl) <= __captbl_cap2bytes(CAP_PGTBL));
	return;
}

int cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr);
int pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte);

#endif /* PGTBL_H */
