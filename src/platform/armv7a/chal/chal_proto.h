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

struct pgtbl_info {
	asid_t  asid;
	pgtbl_t pgtbl;
} __attribute__((packed));

/* identical to the capability structure */
struct cap_pgtbl {
	struct cap_header h;
	u32_t             refcnt_flags; /* includes refcnt and flags */
	pgtbl_t           pgtbl;
	u32_t             lvl;       /* what level are the pgtbl nodes at? */
	struct cap_pgtbl *parent;    /* if !null, points to parent cap */
	u64_t             frozen_ts; /* timestamp when frozen is set. */
} __attribute__((packed));

static inline void
chal_pgtbl_update(struct pgtbl_info *ptinfo)
{
	/*
	 * https://github.com/gwsystems/composite/commit/6c41838513f6b5188a5b0353ff3c1f6c19c6fff5
	 * In this commit, I debugged different variants for asid context switch code 
	 * but wasn't successful with any of those. 
	 * Not sure what I did wrong.. Now this woks
	 */
	paddr_t ttbr0 = chal_va2pa(ptinfo->pgtbl) | 0x4a;

	/* asm volatile("mcr p15, 0, r0, c8, c7, 0"); was using TLBIALL */
	
	/* Scheme 1 - cycle through an unused ASID - slower 
	asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (0));
	asm volatile("isb");
	asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (ttbr0));
	asm volatile("isb");
	asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (ptinfo->asid));
	 */
	extern unsigned char __cos_cav7_kern_pgtbl;
	extern unsigned char __va_offset__;

/* #define TTBR1_CONTENT 	((&__cos_cav7_kern_pgtbl - &__va_offset__) | 0x4a) */
#define TTBR1_CONTENT  (0x0015004a)

	/* Scheme 2 - cycle through global page table - faster - with separated inline asms, which GCC is not good at.
	asm volatile("dsb");
	asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (TTBR1_CONTENT));
	asm volatile("isb");
	asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (ptinfo->asid));
	asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (ttbr0));
	asm volatile("isb");  */
	
	/* Scheme 2 - cycle through global page table - faster - whole blocks.
	__asm__ __volatile__("dsb \n\t"
	                     "mcr p15, 0, %0, c2, c0, 0 \n\t"
	                     "isb \n\t"
	                     "mcr p15, 0, %1, c13, c0, 1  \n\t"
	                     "mcr p15, 0, %2, c2, c0, 0 \n\t"
	                     "isb \n\t"
	                     :: "r"(TTBR1_CONTENT), "r"(ptinfo->asid), "r"(ttbr0));
}

extern asid_t free_asid;
static inline asid_t
chal_asid_alloc(void)
{
	if (unlikely(free_asid >= MAX_NUM_ASID)) assert(0);
	return cos_faa((int *)&free_asid, 1);
}

#endif /* CHAL_PROTO_H */
