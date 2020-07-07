/* Architecture-specific prototypes that go into the kernel */
#ifndef CHAL_PROTO_H
#define CHAL_PROTO_H

/* Page table platform-dependent definitions - for ARM, some of the x86 page flags are not useful */
#define PGTBL_PGTIDX_SHIFT (20)
#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FRAME_BITS (32 - PGTBL_PAGEIDX_SHIFT)
#define PGTBL_FLAG_MASK ((1 << PGTBL_PAGEIDX_SHIFT) - 1)
#define PGTBL_FRAME_MASK (~PGTBL_FLAG_MASK)
#define PGTBL_DEPTH 2
#define PGTBL_ENTRY_ORDER 8
#define PGTBL_ENTRY (1 << PGTBL_ENTRY_ORDER)
#define SUPER_PAGE_FLAG_MASK  (0x3FFFFF)
#define SUPER_PAGE_PTE_MASK   (0x3FF000)

/* FIXME:find a better way to do this */
#define EXTRACT_SUB_PAGE(super) ((super) & SUPER_PAGE_PTE_MASK)

struct pgtbl
{
	u32_t 		pgtbl[1024];
};

/* Page table related prototypes & structs */
/* make it an opaque type...not to be touched */
typedef struct pgtbl *pgtbl_t;

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

