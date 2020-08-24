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
// What Fiasco does
//  asm volatile (
//      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
//      "dsb                          \n"
//      "mcr p15, 0, %2, c13, c0, 1   \n" // change ASID to 0
//      "isb                          \n"
//      "mcr p15, 0, %0, c2, c0       \n" // set TTBR
//      "isb                          \n"
//      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
//      "isb                          \n"
//      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
//      "isb                          \n"
//      "mov r1, r1                   \n"
//      "sub pc, pc, #4               \n"
//      :
//      : "r" (chal_va2pa(ptinfo->pgtbl) | 0x6a), "r" (ptinfo->asid), "r" (0)
//      : "r1" );
	// 6a == S Sharable | RGN = Outer WB-WA | IRGN = Inner WB-WA | NOS

	paddr_t ttbr0 = chal_va2pa(ptinfo->pgtbl) | 0x4a;
//
//	///* DEBUG */
//	//unsigned long currasid = 0;
//	//asm volatile("mrc p15, 0, %0, c13, c0, 1" : "=r" (currasid));
//	//printk("(START) Curr ASID: %lx, Next ASID: %lx\n", ctxtidr, ptinfo->asid);
//
	/* Without ASIDs START */
	asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (ttbr0));
	asm volatile("mcr p15, 0, r0, c8, c7, 0"); /* TLBIALL */
	/* Without ASIDs END */
//
//	///* Example 3-5 START */
//	//unsigned long ttbcr0 = 0, ttbcr1 = 0, ttbcr2 = 0, ttbcr3 = 0, ttbcr4 = 0;
//	//asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r" (ttbcr0));
//	//ttbcr4 = ttbcr0;
//	//ttbcr0 |= TTBCR_PD0;
//	//asm volatile("mcr p15, 0, %0, c2, c0, 2" :: "r" (ttbcr0));
//	////asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r" (ttbcr1));
//	//asm volatile("isb");
//	///* NOTE: PROCID unused */
//	//asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (ptinfo->asid));
//	//asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (ttbr0));
//	//asm volatile("isb");
//	//asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r" (ttbcr2));
//	//ttbcr3 = ttbcr2;
//	//ttbcr3 &= ~TTBCR_PD0;
//	//asm volatile("mcr p15, 0, %0, c2, c0, 2" :: "r" (ttbcr3));
//	/* Example 3-5 END */
//	//printk("%d: TTBCR start :%lx, set PD0:%lx, read after set:%lx, read after isb:%lx, last set: %lx\n", __LINE__, ttbcr4, ttbcr0, ttbcr1, ttbcr2, ttbcr3);
//	 
//	/* Example 3-3 START */
//	//asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (0));
//	//asm volatile("isb");
//	//asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (ttbr0));
//	//asm volatile("isb");
//	///* NOTE: PROCID unused */
//	//asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (ptinfo->asid));
//	/* Example 3-3 END */
//
//	/* DEBUG */
//	//asm volatile("mrc p15, 0, %0, c13, c0, 1" : "=r" (ctxtidr));
//	//printk("(END) Current ASID: %lx\n", ctxtidr);
//
//	/* Lets mimic cpu_v7_switch_mm from Linux */
//	/* ARM ERRATA 754322 */
//	dsb();
//	asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (globalpd)); 
//	isb();
//	/* ARM ERRATA 430973 */
//	//asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(0));
//	asm volatile("mcr p15, 0, %0, c13, c0, 1" :: "r" (ptinfo->asid));
//	isb();
//	asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r" (ttbr0)); 
//	isb();
//	//asm volatile("mcr p15, 0, %0, c8, c5, 0" :: "r" (0)); /* ITLBIALL */
//	//asm volatile("mcr p15, 0, %0, c8, c7, 0" :: "r" (0)); /* TLBIALL */
//	//asm volatile (	"dsb\n\t"
//	//		"mcr p15, 0, %[_ctxtidr], c13, c0, 1\n\t"
//	//		"isb\n\t"
//	//		"mcr p15, 0, %[_ttbr0], c2, c0, 0\n\t"
//	//		"isb\n\t"
//	//		:
//	//		: [_ctxtidr] "m" (ptinfo->asid), [_ttbr0] "m" (ttbr0)
//	//		: "memory", "cc"
//	//	     );
//
}

extern asid_t free_asid;
static inline asid_t
chal_asid_alloc(void)
{
	if (unlikely(free_asid >= MAX_NUM_ASID)) assert(0);
	return cos_faa((int *)&free_asid, 1);
}

#endif /* CHAL_PROTO_H */
