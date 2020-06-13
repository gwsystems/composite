/* Architecture-specific prototypes that go into the kernel */
#ifndef CHAL_PROTO_H
#define CHAL_PROTO_H

/* Give up ertrie on this platform */
/* Page table platform-dependent definitions */
#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FRAME_BITS (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK ((1 << PGTBL_PAGEIDX_SHIFT) - 1)
#define PGTBL_FRAME_MASK (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH 2
#define PGTBL_ORD 10

/* FIXME:find a better way to do this */
#define EXTRACT_SUB_PAGE(super) ((super) & SUPER_PAGE_PTE_MASK)

/* Page table related prototypes & structs */
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

/* make it an opaque type...not to be touched */
typedef struct pgtbl pgtbl_t;

/* identical to the capability structure */
struct cap_pgtbl {
	struct cap_header h;
	u32_t             refcnt_flags; /* includes refcnt and flags */
	pgtbl_t           pgtbl;
	u32_t             lvl;       /* what level are the pgtbl nodes at? */
	struct cap_pgtbl *parent;    /* if !null, points to parent cap */
	u64_t             frozen_ts; /* timestamp when frozen is set. */
} __attribute__((packed));
#endif /* CHAL_PROTO_H */

