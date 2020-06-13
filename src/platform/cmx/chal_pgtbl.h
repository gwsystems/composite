/* This is for Cortex-M */
typedef enum {
	CMX_PGTBL_PRESENT	= 1,
	CMX_PGTBL_LEAF		= 1<<1,
	CMX_PGTBL_WRITABLE	= 1<<2,
	CMX_PGTBL_USER		= 1<<3,
	CMX_PGTBL_WT		= 1<<4, 	/* write-through caching */
	CMX_PGTBL_NOCACHE	= 1<<5, 	/* caching disabled */
	/* Composite defined bits next*/
	CMX_PGTBL_COSFRAME	= 1<<6,
	CMX_PGTBL_COSKMEM	= 1<<7,     /* page activated as kernel object */
	/* Flag bits done. */
	CMX_PGTBL_USER_DEF	= CMX_PGTBL_PRESENT|CMX_PGTBL_LEAF|CMX_PGTBL_USER|CMX_PGTBL_WRITABLE|CMX_PGTBL_WT|CMX_PGTBL_COSFRAME,
	CMX_PGTBL_INTERN_DEF	= CMX_PGTBL_PRESENT,
} chal_pgtbl_flags_t;

#define COS_PGTBL_USER_DEF	CMX_PGTBL_USER_DEF
#define COS_PGTBL_INTERN_DEF	CMX_PGTBL_INTERN_DEF

/* Make these reconfigurable too */
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

/* Whether this node contains metadata or it is a internal node */
#define COS_PGTBL_MPUMETA					0x00
#define COS_PGTBL_INTERN					0x01

/* Get the metadata's address from the page table itself */
#define COS_PGTBL_METAADDR(X)					((X)&(~(0x03)))

#define COS_PGTBL_NODE_ADDR(X)					((X)&(0xFFFFFF00))
#define COS_PGTBL_SIZEORDER(X)					((((X)&0xFF)>>3)+7)
#define COS_PGTBL_NUMORDER(X)					((((X)&0x07)>>1)+1)
#define COS_PGTBL_TYPE(X)					((X)&0x01)

#define COS_PGTBL_TYPEADDR(addr,size_order,num_order,type)	(((addr)&(~0xFF))|((size_order-7)<<3)|((num_order-1)<<1)|(type))
#define COS_PGTBL_CHILD(addr,flags)				((((u32_t)(addr))&(~0x03))|(flags))
#define COS_PGTBL_LEAF(addr,flags)				(((addr)&(~0xFF))|(flags))

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

/* PRY:the page table lookup function - look up for a leaf, its size order, flags, and what final page table layer contains it.
 * COSFRAME will also be returned, not only mapped pages */
static u32_t*
pgtbl_lkup_leaf(pgtbl_t pt, vaddr_t addr, u32_t* size_order, u32_t* flags, pgtbl_t* contain)
{
	u32_t start_addr;
	u32_t excess_addr;
	u32_t sz_order;

	/* The pgtbl_t is empty */
	if(pt == 0)
		return 0;

	/* what is this pt? If this one contains MPU metadata, then we look for its next level */
	if(COS_PGTBL_TYPE(pt->type_addr) == COS_PGTBL_MPUMETA)
		pt = (pgtbl_t)COS_PGTBL_METAADDR(pt->type_addr);

	while(1) {
		/* Get the start address of this */
		start_addr = COS_PGTBL_NODE_ADDR(pt->type_addr);
		/* Start translation */
		excess_addr = addr-start_addr;
		sz_order = COS_PGTBL_SIZEORDER(pt->type_addr);
		/* Not in range */
		if((excess_addr >> sz_order) >= (1 << COS_PGTBL_NUMORDER(pt->type_addr)))
			return 0;

		/* See if this entry exists. If not, we return directly */
		if((pt->data.pgt[excess_addr >> sz_order] & CMX_PGTBL_PRESENT) == 0)
			break;
		/* This exists. See what is in this entry. Is this a pte or an internal node */
		if((pt->data.pgt[excess_addr >> sz_order] & CMX_PGTBL_LEAF) != 0)
			break;

		/* Must be an internal node */
		pt=(pgtbl_t)COS_PGTBL_METAADDR(pt->data.pgt[excess_addr >> sz_order]);
	}
	if(size_order!=0)
		*size_order = sz_order;
	if(flags!=0)
		*flags = (pt->data.pgt[excess_addr>>sz_order] & PGTBL_FLAG_MASK);
	if(contain!=0)
		*contain = pt;
	return &(pt->data.pgt[excess_addr>>sz_order]);
}

static u32_t*
pgtbl_lkup_slot(pgtbl_t pt, vaddr_t expandid)
{
	/* The pgtbl_t is empty */
	if(pt == 0)
		return 0;

	/* what is this pt? If this one contains MPU metadata, then we just return an error */
	if(COS_PGTBL_TYPE(pt->type_addr) == COS_PGTBL_MPUMETA)
		return 0;

	/* Not in range */
	if(expandid > (1 << COS_PGTBL_NUMORDER(pt->type_addr)))
		return 0;

	return &(pt->data.pgt[expandid]);
}

static u32_t
_pgtbl_isnull(u32_t pte)
{
	if(pte==0)
		return 1;
	else
		return 0;
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
	u32_t* pte;
	int ret = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* Get the pte */
	pte=pgtbl_lkup_leaf(pt, addr, 0, 0, 0);

	if (!pte) return -ENOENT;

	if (*pte & CMX_PGTBL_PRESENT)  return -EEXIST;
	if (*pte & CMX_PGTBL_COSFRAME) return -EPERM;

	/* ref cnt on the frame - we assume that this will succeed */
	ret = retypetbl_ref((void *)page);
	if (ret) return ret;

	*pte|=flags;
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

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte=(u32_t*)pgtbl_lkup_leaf(pt, addr, 0, 0, 0);

	if (*pte & CMX_PGTBL_PRESENT)  return -EEXIST;
	if (*pte & CMX_PGTBL_COSFRAME) return -EPERM;

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
	u32_t orig_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte=(u32_t*)pgtbl_lkup_leaf(pt, addr, 0, 0, 0);
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
	u32_t orig_v, accum = 0;

	assert(pt && prevflags);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte = (u32_t*)pgtbl_lkup_leaf(pt, addr, 0, 0, 0);
	if (!(*pte & CMX_PGTBL_PRESENT))  return -ENOENT;
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

	unsigned long orig_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* In pgtbl, we have only 20bits for liv id. */
	if (unlikely(liv_id >= (1 << (32-PGTBL_PAGEIDX_SHIFT)))) return -EINVAL;

	/* Liveness tracking of the unmapping VAS. */
	ret = ltbl_timestamp_update(liv_id);
	if (unlikely(ret)) goto done;

	/* get the pte */
	pte = pgtbl_lkup_leaf(pt, addr, 0, 0, 0);
	if (!(*pte & CMX_PGTBL_PRESENT)) return -EEXIST;
	if (*pte & CMX_PGTBL_COSFRAME)   return -EPERM;

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

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* get the pte */
	pte=(u32_t*)pgtbl_lkup_leaf(pt, addr, 0, 0, 0);
	*pte=0;

	return 0;
}

static int
pgtbl_ispresent(u32_t flags)
{
	return flags & (CMX_PGTBL_PRESENT|CMX_PGTBL_COSFRAME);
}

static int
pgtbl_get_cosframe(pgtbl_t pt, vaddr_t frame_addr, paddr_t *cosframe)
{
	u32_t *pte;
	paddr_t v;

	pte =(u32_t*) pgtbl_lkup_leaf(pt, frame_addr, 0, 0, 0);
	if (!pte) return -EINVAL;

	v = *pte;
	if (!(v & CMX_PGTBL_COSFRAME)) return -EINVAL;

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
			     "movw r0,#0xED9C \n\t" \
			     "movt r0,#0xE000 \n\t" \
			     "stm r0!,{r1-r8}; \n\t" \
			     "dsb \n\t" \
			     : \
			     : [_pgtbl]"m"(pt) \
			     : "r0","r1","r2","r3","r4","r5","r6","r7","r8","memory", "cc");
}

/* vaddr -> kaddr, PRY:guess that we are returning flags and size order as well? */
static vaddr_t
pgtbl_translate(pgtbl_t pt, u32_t addr, u32_t* size_order, u32_t *flags)
{
	u32_t* pte;

	pte =(u32_t*) pgtbl_lkup_leaf(pt, addr, size_order, flags, 0);
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
