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
#define NUM_ASID_BITS (12)
#define NUM_ASID_MAX ((1<<NUM_ASID_BITS)-1)
#define PGTBL_ASID_MASK (0xfff)
#define CR3_NO_FLUSH (1ul << 63)
#elif defined(__i386__)
#define PGTBL_ENTRY_ADDR_MASK 0xfffff000
#define PGTBL_DEPTH 2
#define PGTBL_ENTRY_ORDER 10
#define PGTBL_FLAG_MASK ((1 << PGTBL_PAGEIDX_SHIFT) - 1)
#define PGTBL_FRAME_MASK (~PGTBL_FLAG_MASK)
#define NUM_ASID_MAX (0)
#define CR3_NO_FLUSH (0) /* this just wont do anything */
#define PGTBL_ASID_MASK (0)
#endif

#define PGTBL_ENTRY (1 << PGTBL_ENTRY_ORDER)
#define SUPER_PAGE_FLAG_MASK  (0x3FFFFF)
#define SUPER_PAGE_PTE_MASK   (0x3FF000)

// #define MPK_ENABLE 1

/* FIXME:find a better way to do this */
#define EXTRACT_SUB_PAGE(super) ((super) & SUPER_PAGE_PTE_MASK)

/* Page table related prototypes & structs */
/* make it an opaque type...not to be touched */
typedef struct pgtbl *pgtbl_t;

struct pgtbl_info {
	pgtbl_t       pgtbl;
	prot_domain_t protdom;
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

#ifdef MPK_ENABLE
static inline void
wrpkru(u32_t pkru)
{
	asm volatile (
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"mov %0,    %%eax\n\t"
		"wrpkru\n\t"
		:
		: "r" (pkru)
		: "eax", "rcx", "rdx"
	);
}

static inline u32_t
rdpkru(void)
{
	u32_t pkru;

	asm volatile(
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"rdpkru"
		: "=a" (pkru)
		:
		: "rcx", "rdx"
	);

	return pkru;
}

static inline u32_t
pkru_state(prot_domain_t protdom)
{
	u16_t mpk_key = PROTDOM_MPK_KEY(protdom);
	return ~(0b11 << (2 * mpk_key)) & ~0b11;
}

static inline void
chal_protdom_write(prot_domain_t protdom)
{
	/* we only update asid on pagetable switch */
	wrpkru(pkru_state(protdom));
}

static inline prot_domain_t
chal_protdom_read(void)
{
	unsigned long cr3;
	u16_t asid, mpk_key;

	u32_t pkru = rdpkru();
	assert(pkru);
	/* inverse of `pkru_state` */
	mpk_key = (32 - __builtin_clz(~pkru)) / 2 - 1;

	asm volatile("mov %%cr3, %0" : "=r"(cr3) : :);
	asid = (u16_t)(cr3 & PGTBL_ASID_MASK);

	return PROTDOM_INIT(asid, mpk_key);
}
#else /* !MPK_ENABLE */
static inline void wrpkru(u32_t pkru) {}
static inline u32_t rdpkru(void) { return 0; }
static inline u32_t pkru_state(prot_domain_t protdom) { return 0; }
static inline void chal_protdom_write(prot_domain_t protdom) {}
static inline prot_domain_t chal_protdom_read(void) { return 0; }
#endif /* MPK_ENABLE */

struct cpu_tlb_asid_map {
	pgtbl_t mapped_pt[NUM_ASID_MAX];
} CACHE_ALIGNED;

extern struct cpu_tlb_asid_map tlb_asid_map[NUM_CPU];

static inline pgtbl_t
chal_cached_pt_curr(prot_domain_t protdom)
{
	u16_t asid = PROTDOM_ASID(protdom);
	return tlb_asid_map[get_cpuid()].mapped_pt[asid];
}

static inline void
chal_cached_pt_update(pgtbl_t pt, prot_domain_t protdom)
{
	u16_t asid = PROTDOM_ASID(protdom);
	tlb_asid_map[get_cpuid()].mapped_pt[asid] = pt;
}

/* Update the page table */
static inline void
chal_pgtbl_update(struct pgtbl_info *pt)
{
	u16_t asid = PROTDOM_ASID(pt->protdom);

	/* lowest 12 bits is the context identifier */
	unsigned long cr3 = (unsigned long)pt->pgtbl | asid;

	/* fastpath: don't need to invalidate tlb entries; otherwise flush tlb on switch */
	if (likely(chal_cached_pt_curr(asid) == pt->pgtbl)) {
		cr3 |= CR3_NO_FLUSH;
	} else {
		chal_cached_pt_update(pt->pgtbl, asid);
	}

	asm volatile("mov %0, %%cr3" : : "r"(cr3));
}

/* Check current page table */
static inline pgtbl_t
chal_pgtbl_read(void)
{
	unsigned long pt;

	asm volatile("mov %%cr3, %0" : "=r"(pt) : :);

	return (pgtbl_t)(pt & PGTBL_ENTRY_ADDR_MASK);
}

#endif /* CHAL_PROTO_H */
