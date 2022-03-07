/* Architecture-specific prototypes that go into the kernel */
#ifndef CHAL_PROTO_H
#define CHAL_PROTO_H

/* Page table platform-dependent definitions */
#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FRAME_BITS (32 - PGTBL_PAGEIDX_SHIFT)

#if defined(__x86_64__)
#define PGTBL_ENTRY_ADDR_MASK 0xfffffffffffff000
#define PGTBL_DEPTH 4
#define PGTBL_ENTRY_ORDER 9
#define PGTBL_FLAG_MASK 0xf800000000000fff
#define PGTBL_FRAME_MASK (~PGTBL_FLAG_MASK)

#define NUM_ASID_BITS 12
#define NUM_ASID_MAX (1<<NUM_ASID_BITS)-1

#elif defined(__i386__)
#define PGTBL_ENTRY_ADDR_MASK 0xfffff000
#define PGTBL_DEPTH 2
#define PGTBL_ENTRY_ORDER 10
#define PGTBL_FLAG_MASK ((1 << PGTBL_PAGEIDX_SHIFT) - 1)
#define PGTBL_FRAME_MASK (~PGTBL_FLAG_MASK)
#endif

#define PGTBL_ENTRY (1 << PGTBL_ENTRY_ORDER)
#define SUPER_PAGE_FLAG_MASK  (0x3FFFFF)
#define SUPER_PAGE_PTE_MASK   (0x3FF000)

/* FIXME:find a better way to do this */
#define EXTRACT_SUB_PAGE(super) ((super) & SUPER_PAGE_PTE_MASK)

/* Page table related prototypes & structs */
/* make it an opaque type...not to be touched */
typedef struct pgtbl *pgtbl_t;

struct pgtbl_info {
	asid_t  asid;
	pgtbl_t pgtbl;
} __attribute__((packed));

/* identical to the capability structure */
struct cap_pgtbl {
	struct cap_header h;
	u32_t             refcnt_flags; /* includes refcnt and flags */
	pgtbl_t           pgtbl;
	u32_t             lvl;          /* what level are the pgtbl nodes at? */
	asid_t            asid;         /* hardware context identifier */
	struct cap_pgtbl *parent;       /* if !null, points to parent cap */
	u64_t             frozen_ts;    /* timestamp when frozen is set. */
} __attribute__((packed));

extern pgtbl_t tlb_asid_active[NUM_CPU][NUM_ASID_MAX];

/* Update the page table */
static inline void
chal_pgtbl_update(struct pgtbl_info *pt)
{	
	pgtbl_t *curr_cached = &tlb_asid_active[get_cpuid()][pt->asid];

	/* currently cached pgtbl is this pgtbl */
	if (*curr_cached == pt->pgtbl) {
		__writecr3((unsigned long)pt->pgtbl | pt->asid | CR3_NO_FLUSH);
		return;
	}

	/* no pgtbl cached for this asid */
	if (*curr_cached == 0) {
		__writecr3((unsigned long)pt->pgtbl | pt->asid | CR3_NO_FLUSH);
	}
	/* different pgtbl cached for this asid, need to invalidate asid */
	else {
		__writecr3((unsigned long)pt->pgtbl | pt->asid);
	}

	*curr_cached = pt->pgtbl;
}

static inline asid_t
chal_asid_alloc(void)
{ 
	return 0;
}

#endif /* CHAL_PROTO_H */

