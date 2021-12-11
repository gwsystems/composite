#ifndef CHAL_PGTBL_H
#define CHAL_PGTBL_H

/* These code below are for x86 specifically, only used in x86 chal */
typedef enum
{
	CAV7_1M_PGDIR_PRESENT   = 0x01U,
	CAV7_1M_PGDIR_NOTSECURE = (1U << 3U),
	CAV7_1M_PAGE_PRESENT    = (0x02U),
	CAV7_1M_BUFFERABLE      = (1U << 2U),
	CAV7_1M_CACHEABLE       = (1U << 3U),
	CAV7_1M_EXECUTENEVER    = (1U << 4U),
	CAV7_1M_ACCESS          = (1U << 10U),
	CAV7_1M_USER            = (1U << 11U),
	CAV7_1M_READONLY        = (1U << 15U),
	CAV7_1M_SHAREABLE       = (1U << 16U),
	CAV7_1M_NOTGLOBAL       = (1U << 17U),
	CAV7_1M_NOTSECURE       = (1U << 19U),

	CAV7_4K_EXECUTENEVER = (1U << 0U),
	CAV7_4K_PAGE_PRESENT = (1U << 1U),
	CAV7_4K_BUFFERABLE   = (1U << 2U),
	CAV7_4K_CACHEABLE    = (1U << 3U),
	CAV7_4K_ACCESS       = (1U << 4U),
	CAV7_4K_USER         = (1U << 5U),
	CAV7_4K_READONLY     = (1U << 9U),
	CAV7_4K_SHAREABLE    = (1U << 10U),
	CAV7_4K_NOTGLOBAL    = (1U << 11U),

	/* Composite defined bits next - the cosframe will be there if the present is empty */
	CAV7_PGTBL_COSFRAME = 1U << 11U,
	/* We enabled TEX remap, thus these two bits are freed */
	CAV7_PGTBL_COSKMEM    = 1U << 8U, /* page activated as kernel object */
	CAV7_PGTBL_QUIESCENCE = 1U << 7U,
	/* Flag bits done. */
	CAV7_4K_USER_COMMON = CAV7_4K_PAGE_PRESENT | CAV7_4K_USER | CAV7_4K_SHAREABLE | CAV7_4K_NOTGLOBAL
	                      | CAV7_4K_ACCESS,
	CAV7_4K_USER_DEF = CAV7_4K_USER_COMMON | CAV7_4K_BUFFERABLE | CAV7_4K_CACHEABLE,

	CAV7_1M_USER_COMMON = CAV7_1M_PAGE_PRESENT | CAV7_1M_USER | CAV7_1M_SHAREABLE | CAV7_1M_NOTGLOBAL
	                      | CAV7_1M_ACCESS,
	CAV7_1M_USER_DEF = CAV7_1M_USER_COMMON | CAV7_1M_BUFFERABLE | CAV7_1M_CACHEABLE,
	CAV7_1M_USER_SEQ = CAV7_1M_USER_COMMON,

	CAV7_1M_KERN_COMMON = CAV7_1M_PAGE_PRESENT | CAV7_1M_SHAREABLE | CAV7_1M_NOTGLOBAL | CAV7_1M_ACCESS,
	CAV7_1M_KERN_DEF    = CAV7_1M_KERN_COMMON | CAV7_1M_BUFFERABLE | CAV7_1M_CACHEABLE,
	CAV7_1M_KERN_SEQ    = CAV7_1M_KERN_COMMON,

	CAV7_1M_INTERN_DEF = CAV7_1M_PGDIR_PRESENT,
} pgtbl_flags_cav7_t;

/* MMU definitions operation flags, assuming the following changes:
 * TTBCR=2 : TTBR0 use 1GB,
 * SCTLR.AFE=1 : AP[2:1] is the permission flags, AP[0] is now the access flag.
 * DACR=0x55555555 : All pages/tables are client and access permissions always checked. 0xFFFFFFFF not checked,
 * SCTLR.TRE=1 : TEX remap engaged, CACHEABLE and BUFFERABLE works as currently defined.
 *               {MSB TEX[0], C, B LSB} will index PRRR and NMRR.
 * PRRR=0b 00 00 00 00 00 00 10 10 00 00 00 00 10 10 01 00=0x000A00A4
 *         OUTER_SHARE PTBL_DECIDE xx xx xx xx CB C- -B --
 *      In PRRR configuration, shareability is decided by the page table. This
 *      facilitates cache-incoherent user-level systems. Also, if the memory
 *      is shareable, then it is both inner and outer shareable.
 * NMRR=0b 00 00 00 00 01 10 11 00 00 00 00 00  01 10 11 00=0x006C006C
 *         xx xx xx xx CB C- -B -- xx xx xx xx  CB C- -B --
 * -- : Non-cacheable non-bufferable - strongly ordered, caching not allowed at all
 * -B : Non-cacheable bufferable - device, write-back without write-allocate (since
 *      reads will still have to load words from memory)
 * C- : Cacheable non-bufferable - normal, write-through without write allocate
 * CB : Cachaable bufferable - normal memory, write-back, write-allocate */

#define CAV7_4G_PGTBL_ADDR(X) ((X)&0xFFFFF000U)
#define CAV7_1M_PGTBL_ADDR(X) ((X)&0xFFFFFC00U)
#define CAV7_1M_PAGE_ADDR(X) ((X)&0xFFF00000U)
#define CAV7_4K_PAGE_ADDR(X) ((X)&0xFFFFF000U)
#define CAV7_4K_PAGE_FLAGS(X) ((X)&0x00000FFFU)

#define CAV7_PGFLG_1M_COS2NAT(X) (CAV7_Pgflg_1M_COS2NAT[X])
#define CAV7_PGFLG_1M_PREPROC(X)                                                                             \
	((((X)&CAV7_1M_READONLY) >> 12) | (((X)&CAV7_1M_EXECUTENEVER) >> 2) | (((X)&CAV7_1M_CACHEABLE) >> 2) \
	 | (((X)&CAV7_1M_BUFFERABLE) >> 2))
#define CAV7_PGFLG_1M_NAT2COS(X) (CAV7_Pgflg_1M_NAT2COS[CAV7_PGFLG_1M_PREPROC(X)])

#define CAV7_PGFLG_4K_RME2NAT(X) (CAV7_Pgflg_4K_RME2NAT[X])
#define CAV7_PGFLG_4K_PREPROC(X)                                                                          \
	((((X)&CAV7_4K_READONLY) >> 6) | (((X)&CAV7_4K_CACHEABLE) >> 1) | (((X)&CAV7_4K_BUFFERABLE) >> 1) \
	 | (((X)&CAV7_4K_EXECUTENEVER) >> 0))
#define CAV7_PGFLG_4K_NAT2RME(X) (CAV7_Pgflg_4K_NAT2RME[CAV7_4K_PREPROC(X)])

/**
 * We only need to do mapping_add at boot time to add all physical
 * memory to the pgtbl of llboot. After that, we only need to do copy
 * from llboot pgtbl to other pgtbls. Thus, when adding to pgtbl, we
 * use physical addresses; when doing copy, we don't need to worry
 * about PA.
 */

/* v should include the desired flags */
static inline int
__pgtbl_setleaf(struct ert_intern *a, void *v)
{
	u32_t new, old;
	old = (u32_t)(a->next);
	new = (u32_t)(v);

	if (!cos_cas((unsigned long *)a, old, new)) return -ECASFAIL;

	return 0;
}

/**
 * This takes an input parameter as the old value of the mapping. Only
 * update when the existing value matches.
 */
static inline int
__pgtbl_update_leaf(struct ert_intern *a, void *v, u32_t old)
{
	u32_t new;

	new = (u32_t)(v);
	if (!cos_cas((unsigned long *)a, old, new)) return -ECASFAIL;
	/* printk("old %x, new %x, addr %x\n", old, new, a); */
	return 0;
}

static int
__pgtbl_isnull(struct ert_intern *a, void *accum, int isleaf)
{
	(void)isleaf;
	(void)accum;
	return !(((u32_t)(a->next)) & (CAV7_1M_PAGE_PRESENT | CAV7_PGTBL_COSFRAME));
}

static int
__pgtbl_resolve(struct ert_intern *a, void *accum, int leaf, u32_t order, u32_t sz)
{
	(void)a;
	(void)leaf;
	(void)order;
	(void)sz;
	*(u32_t *)accum = (((u32_t)a->next) & PGTBL_FLAG_MASK);
	return 1;
}

static void
__pgtbl_init(struct ert_intern *a, int isleaf)
{
	(void)isleaf;
	a->next = NULL;
}

#endif /* CHAL_PGTBL_H */
