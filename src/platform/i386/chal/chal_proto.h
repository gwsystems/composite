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
	prot_domain_t protdom;
	pgtbl_t       pgtbl;
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
wrpkru(u32_t pkru)
{
	asm volatile (
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"mov %0,    %%eax\n\t"
		"wrpkru\n\t"
		: : "r" (pkru)
	);
}

static inline u32_t
rdpkru(void)
{
	u32_t pkru = 0;

	asm volatile(
		"xor %%rcx, %%rcx\n\t"
        "xor %%rdx, %%rdx\n\t"
        "rdpkru"
        : : "a" (pkru) :
	);

	return pkru;
}

static inline u32_t
pkru_state(u8_t mpk_key)
{
	return ~(0b11 << (2 * mpk_key)) & ~0b11;
}

static inline void
pkey_enable(u8_t mpk_key)
{
	/* set all keys except the inputted key and key 0 to WD/AD */
	wrpkru(pkru_state(mpk_key));
}

/* Update the page table */
static inline void
chal_pgtbl_update(struct pgtbl_info *pt)
{
	asm volatile("mov %0, %%cr3" : : "r"(pt->pgtbl));
	pkey_enable(pt->protdom);
}

static inline void
chal_pgtbl_update_pkru(pgtbl_t pgtbl, u32_t pkru)
{
	asm volatile("mov %0, %%cr3" : : "r"(pgtbl));
	wrpkru(pkru);
}

static inline asid_t
chal_asid_alloc(void)
{ return 0; }

#endif /* CHAL_PROTO_H */

