/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef PGTBL_H
#define PGTBL_H

typedef enum {
	PGTBL_PRESENT  = 1,
	PGTBL_WRITABLE = 1 << 1,
	PGTBL_USER     = 1 << 2,
	PGTBL_WT       = 1 << 3, /* write-through caching */
	PGTBL_NOCACHE  = 1 << 4, /* caching disabled */
	PGTBL_ACCESSED = 1 << 5,
	PGTBL_MODIFIED = 1 << 6,
	PGTBL_SUPER    = 1 << 7, /* super-page (4MB on x86-32) */
	PGTBL_GLOBAL   = 1 << 8,
	/* Composite defined bits next*/
	PGTBL_COSFRAME   = 1 << 9,
	PGTBL_COSKMEM    = 1 << 10, /* page activated as kernel object */
	PGTBL_QUIESCENCE = 1 << 11,
	/* Flag bits done. */

	PGTBL_USER_DEF   = PGTBL_PRESENT | PGTBL_USER | PGTBL_ACCESSED | PGTBL_MODIFIED | PGTBL_WRITABLE,
	PGTBL_INTERN_DEF = PGTBL_USER_DEF,
} pgtbl_flags_t;

#include "shared/cos_errno.h"
#include "ertrie.h"
#include "shared/util.h"
#include "captbl.h"
#include "retype_tbl.h"
#include "liveness_tbl.h"
#include "chal/defs.h"
#include "chal/chal_prot.h"

#ifndef LINUX_TEST
#include "chal.h"
#endif

/* The page size definitions for composite OS */
typedef enum {
	PGSZ_128B = 7,
	PGSZ_256B = 8,
	PGSZ_512B = 9,
	PGSZ_1K	  = 10,
	PGSZ_2K	  = 11,
	PGSZ_4K	  = 12,
	PGSZ_8K	  = 13,
	PGSZ_16K  = 14,
	PGSZ_32K  = 15,
	PGSZ_64K  = 16,
	PGSZ_128K = 17,
	PGSZ_256K = 18,
	PGSZ_512K = 19,
	PGSZ_1M	  = 20,
	PGSZ_2M	  = 21,
	PGSZ_4M	  = 22
} pgtbl_sz_t;

/* The number of pages in this page table */
typedef enum {
	PGNO_2    = 1,
	PGNO_4	  = 2,
	PGNO_8    = 3,
	PGNO_16   = 4,
	PGNO_32   = 5,
	PGNO_64   = 6,
	PGNO_128  = 7,
	PGNO_256  = 8,
	PGNO_512  = 9,
	PGNO_1K	  = 10,
	PGNO_2K	  = 11,
	PGNO_4K	  = 12,
	PGNO_8K	  = 13,
	PGNO_16K  = 14,
	PGNO_32K  = 15,
	PGNO_64K  = 16
} pgtbl_no_t;

struct tlb_quiescence {
	/* Updated by timer. */
	u64_t last_periodic_flush;
	/* Updated by tlb flush IPI. */
	u64_t last_mandatory_flush;
	/* cacheline size padding. */
	u8_t __padding[CACHE_LINE - 2 * sizeof(u64_t)];
} __attribute__((aligned(CACHE_LINE), packed));

extern struct tlb_quiescence tlb_quiescence[NUM_CPU] CACHE_ALIGNED;

int tlb_quiescence_check(u64_t timestamp);


int            pgtbl_cosframe_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags);
int            pgtbl_mapping_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags);
int            pgtbl_mapping_mod(pgtbl_t pt, u32_t addr, u32_t flags, u32_t *prevflags);
int            pgtbl_mapping_del(pgtbl_t pt, u32_t addr, u32_t liv_id);
int            pgtbl_mapping_del_direct(pgtbl_t pt, u32_t addr);
void          *pgtbl_lkup_lvl(pgtbl_t pt, u32_t addr, u32_t *flags, u32_t start_lvl, u32_t end_lvl);
int            pgtbl_ispresent(u32_t flags);
unsigned long *pgtbl_lkup(pgtbl_t pt, u32_t addr, u32_t *flags);
unsigned long *pgtbl_lkup_pte(pgtbl_t pt, u32_t addr, u32_t *flags);
int            pgtbl_get_cosframe(pgtbl_t pt, vaddr_t frame_addr, paddr_t *cosframe);
vaddr_t        pgtbl_translate(pgtbl_t pt, u32_t addr, u32_t *flags);
pgtbl_t        pgtbl_create(void *page, void *curr_pgtbl);
int            pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl);
int            pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, livenessid_t lid,
                                capid_t pgtbl_cap, capid_t cosframe_addr, const int root);
int            pgtbl_mapping_scan(struct cap_pgtbl *pt);
int            pgtbl_quie_check(u32_t orig_v);
void           pgtbl_init_pte(void *pte);

extern unsigned long __cr3_contents;

static inline void
pgtbl_update(pgtbl_t pt)
{
	asm volatile("mov %0, %%cr3" : : "r"(pt));
}

/* FIXME: this should be using cos_config.h defines */
#define KERNEL_PGD_REGION_OFFSET (PAGE_SIZE - PAGE_SIZE / 4)
#define KERNEL_PGD_REGION_SIZE (PAGE_SIZE / 4)

static void
pgtbl_init(void)
{
	assert(sizeof(struct cap_pgtbl) <= __captbl_cap2bytes(CAP_PGTBL));

	return;
}

int cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr);
int pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte);

#endif /* PGTBL_H */
