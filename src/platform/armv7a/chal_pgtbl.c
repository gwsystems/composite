/**
 * Copyright 2018 by Runyu Pan, panrunyu@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * For ARM Cortex-A v7, the page table can span 4 pages. This is a
 * problem, and we are forced to use TTBR1 to reduce the page table
 * size to fit in a single page. Thus, the user VA is restricted to
 * 0x00000000 - 0x3FFFFFFF, and the kernel VA is restricted to
 * 0xC0000000 - 0xFFFFFFFF. It might be possible to lift these
 * restrictions in the future after we allow a kernel object to be
 * larger than a single page.
 */
#include "shared/cos_types.h"
#include "captbl.h"
#include "pgtbl.h"
#include "cap_ops.h"
#include "liveness_tbl.h"
#include "retype_tbl.h"
#include "chal/chal_proto.h"
#include "chal_pgtbl.h"

/* The memory information to fill in as the initial page table - This
 * is only used during the booting phase. We will map everything
 * as an identical mapping to the kernel space and the user space.
 * TTBR0 and TTBR1 will point to the same page table at this point.
 * We will always set the access flag because that can cause an access
 * fault. This is very bad; we are letting this go.
 * 0x00000000 -> 0x00000000 0x400 pages, user-level
 * 0x00000000 -> 0x80000000 0x800 pages, kernel */
#define CAV7_MEM_ENTRIES (5 * 4U + 1U) * 4U
#define CAV7_MEM_CONTENTS                                                                                             \
	/* Number of entries */                                                                                       \
	CAV7_MEM_ENTRIES, /* Start_PA    Start_VA    Num             Attributes   0xc02      */                       \
	  0x00000000, 0x00000000, 0x400, CAV7_1M_USER_DEF /* User memory - 1008 MB DDR2 SDRAM identically mapped */,  \
	  0x40000000, 0x40000000, 0x400, CAV7_1M_USER_SEQ /* Kernel devices - 512MB device space */, 0x00000000,      \
	  0x80000000, 0x400, CAV7_1M_KERN_DEF /* Kernel memory - 1023MB DDR2 SDRAM */, 0x40100000, 0xC0100000, 0x1FF, \
	  CAV7_1M_USER_SEQ /* Kernel devices - 512MB device space */, 0xE0000000, 0xE0000000, 0x200,                  \
	  CAV7_1M_USER_SEQ /* Kernel devices - 512MB Cortex device space */

unsigned long cos_cav7_mem_info[CAV7_MEM_ENTRIES] = {CAV7_MEM_CONTENTS};

//0x00000000, 0xC0000000, 0x1,   CAV7_1M_KERN_DEF /* Kernel memory - 192kB OCSRAM */,
/* Convert the flags from composite standard to ARM standard */
unsigned long
chal_pgtbl_flag_cos2nat_1M(unsigned long input)
{
	unsigned long output = CAV7_1M_ACCESS | CAV7_1M_SHAREABLE | CAV7_1M_USER;

	if ((input & PGTBL_PRESENT) != 0) output |= CAV7_1M_PAGE_PRESENT;
	if ((input & PGTBL_WRITABLE) == 0) output |= CAV7_1M_READONLY;
	if ((input & PGTBL_WT) == 0) output |= CAV7_1M_BUFFERABLE;
	if ((input & PGTBL_NOCACHE) == 0) output |= CAV7_1M_CACHEABLE;

	/* Convert composite specific bits */
	if ((input & PGTBL_COSFRAME) != 0) output |= CAV7_PGTBL_COSFRAME;
	if ((input & PGTBL_COSKMEM) != 0) output |= CAV7_PGTBL_COSKMEM;
	if ((input & PGTBL_QUIESCENCE) != 0) output |= CAV7_PGTBL_QUIESCENCE;

	return output;
}

/* Convert the flags from composite standard to ARM standard */
unsigned long
chal_pgtbl_flag_cos2nat_4K(unsigned long input)
{
	unsigned long output = CAV7_4K_ACCESS | CAV7_4K_SHAREABLE | CAV7_4K_USER;

	if ((input & PGTBL_PRESENT) != 0) output |= CAV7_4K_PAGE_PRESENT;
	if ((input & PGTBL_WRITABLE) == 0) output |= CAV7_4K_READONLY;
	if ((input & PGTBL_WT) == 0) output |= CAV7_4K_BUFFERABLE;
	if ((input & PGTBL_NOCACHE) == 0) output |= CAV7_4K_CACHEABLE;

	/* Convert composite specific bits */
	if ((input & PGTBL_COSFRAME) != 0) output |= CAV7_PGTBL_COSFRAME;
	if ((input & PGTBL_COSKMEM) != 0) output |= CAV7_PGTBL_COSKMEM;
	if ((input & PGTBL_QUIESCENCE) != 0) output |= CAV7_PGTBL_QUIESCENCE;

	return output;
}

unsigned long
chal_pgtbl_flag_nat2cos_1M(unsigned long input)
{
	unsigned long output = PGTBL_SUPER | PGTBL_USER;

	if ((input & CAV7_1M_PAGE_PRESENT) != 0) output |= PGTBL_PRESENT;
	if ((input & CAV7_1M_READONLY) == 0) output |= PGTBL_WRITABLE;
	if ((input & CAV7_1M_BUFFERABLE) == 0) output |= PGTBL_WT;
	if ((input & CAV7_1M_CACHEABLE) == 0) output |= PGTBL_NOCACHE;

	/* Convert composite specific bits */
	if ((input & CAV7_1M_PAGE_PRESENT) == 0) {
		/* When not present, this serves as cosframe */
		if ((input & CAV7_PGTBL_COSFRAME) != 0) output |= PGTBL_COSFRAME;
	}
	if ((input & CAV7_PGTBL_COSKMEM) != 0) output |= PGTBL_COSKMEM;
	if ((input & CAV7_PGTBL_QUIESCENCE) != 0) output |= PGTBL_QUIESCENCE;

	return output;
}

unsigned long
chal_pgtbl_flag_nat2cos_4K(unsigned long input)
{
	unsigned long output = PGTBL_USER;

	if ((input & CAV7_4K_PAGE_PRESENT) != 0) output |= PGTBL_PRESENT;
	if ((input & CAV7_4K_READONLY) == 0) output |= PGTBL_WRITABLE;
	if ((input & CAV7_4K_BUFFERABLE) == 0) output |= PGTBL_WT;
	if ((input & CAV7_4K_CACHEABLE) == 0) output |= PGTBL_NOCACHE;

	/* Convert composite specific bits */
	if ((input & CAV7_4K_PAGE_PRESENT) == 0) {
		/* When not present, this serves as cosframe */
		if ((input & CAV7_PGTBL_COSFRAME) != 0) output |= PGTBL_COSFRAME;
	}
	if ((input & CAV7_PGTBL_COSKMEM) != 0) output |= PGTBL_COSKMEM;
	if ((input & CAV7_PGTBL_QUIESCENCE) != 0) output |= PGTBL_QUIESCENCE;

	return output;
}

/*
 * These functions do flag operations - no superpage support is included because
 * the pages are indistinguishable from each other by only looking at the entry itself
 */
unsigned long
chal_pgtbl_flag_add(unsigned long input, pgtbl_flags_t flags)
{
	unsigned long output;

	output = chal_pgtbl_flag_nat2cos_4K(input) | flags;
	return chal_pgtbl_flag_cos2nat_4K(output) | CAV7_4K_PAGE_ADDR(input);
}

unsigned long
chal_pgtbl_flag_clr(unsigned long input, pgtbl_flags_t flags)
{
	unsigned long output;

	output = chal_pgtbl_flag_nat2cos_4K(input) & (~flags);
	return chal_pgtbl_flag_cos2nat_4K(output) | CAV7_4K_PAGE_ADDR(input);
}

unsigned long
chal_pgtbl_flag_exist(unsigned long input, pgtbl_flags_t flags)
{
	return chal_pgtbl_flag_nat2cos_4K(input) & (flags);
}

unsigned long
chal_pgtbl_flag_all(unsigned long input, pgtbl_flags_t flags)
{
	return chal_pgtbl_flag_exist(input, flags) == flags;
}

unsigned long
chal_pgtbl_frame(unsigned long input)
{
	return CAV7_4K_PAGE_ADDR(input);
}

unsigned long
chal_pgtbl_flag(unsigned long input)
{
	return CAV7_4K_PAGE_FLAGS(input);
}

/* Perform page table walks in the ARM architecture */
unsigned long *
__chal_pgtbl_lkup(pgtbl_t pt, unsigned long addr)
{
	unsigned long *next;

	if (addr >= 0x40000000) return 0;

	next = (unsigned long *)(((unsigned long *)pt)[addr >> PGTBL_PGTIDX_SHIFT]);

	if ((((unsigned long)next) & CAV7_1M_PGDIR_PRESENT) == 0) return 0;

	next = (unsigned long *)chal_pa2va(CAV7_1M_PGTBL_ADDR((unsigned long)next));
	return &(next[(addr >> PGTBL_PAGEIDX_SHIFT) & 0xFF]);
}

int
chal_pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret)
{
	struct ert_intern *pte;
	u32_t              orig_v, new_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* Get the pte */
	pte = (struct ert_intern *)__chal_pgtbl_lkup(pt, addr);
	if (unlikely(!pte)) return -ENOENT;
	if (unlikely(__pgtbl_isnull(pte, 0, 0))) return -ENOENT;

	orig_v = (u32_t)(pte->next);
	if (unlikely(!(orig_v & CAV7_PGTBL_COSFRAME))) return -EINVAL; /* can't activate non-frames */
	if (unlikely(orig_v & CAV7_PGTBL_COSKMEM)) return -EEXIST;     /* can't re-activate kmem frames */
	assert(!(orig_v & CAV7_PGTBL_QUIESCENCE));

	*kern_addr = (unsigned long)chal_pa2va((paddr_t)(orig_v & PGTBL_FRAME_MASK));
	new_v      = orig_v | CAV7_PGTBL_COSKMEM;

	/* pa2va (value in *kern_addr) will return NULL if the page is not kernel accessible */
	if (unlikely(!*kern_addr)) return -EINVAL; /* cannot retype a non-kernel accessible page */
	/* Now we can only activate 4k pages */
	if (unlikely(retypetbl_kern_ref((void *)(new_v & PGTBL_FRAME_MASK), PAGE_ORDER))) return -EFAULT;

	/*
	 * We keep the cos_frame entry, but mark it as COSKMEM so that
	 * we won't use it for other kernel objects.
	 */
	if (unlikely(cos_cas((unsigned long *)pte, orig_v, new_v) != CAS_SUCCESS)) {
		/* restore the ref cnt. */
		retypetbl_kern_deref((void *)(orig_v & PGTBL_FRAME_MASK), PAGE_ORDER);
		return -ECASFAIL;
	}
	/*
	 * Return the pte ptr, so that we can release the page if the
	 * kobj activation failed later.
	 */
	*pte_ret = (unsigned long *)pte;

	return 0;
}

int
chal_tlb_quiescence_check(u64_t timestamp)
{
	int i, quiescent = 1;

	/*
	 * Did timer interrupt (which does tlb flush
	 * periodically) happen after unmap? The periodic
	 * flush happens on all cpus, thus only need to check
	 * the time stamp of the current core for that case
	 * (assuming consistent time stamp counters).
	 */
	if (timestamp > tlb_quiescence[get_cpuid()].last_periodic_flush) {
		/*
		 * If no periodic flush done yet, did the
		 * mandatory flush happen on all cores?
		 */
		for (i = 0; i < NUM_CPU_COS; i++) {
			if (timestamp > tlb_quiescence[i].last_mandatory_flush) {
				/* no go */
				quiescent = 0;
				break;
			}
		}
	}
	if (quiescent == 0) {
		printk("from cpu %d, t %llu: cpu %d last mandatory flush: %llu\n", get_cpuid(), timestamp, i,
		       tlb_quiescence[i].last_mandatory_flush);
		for (i = 0; i < NUM_CPU_COS; i++) {
			printk("cpu %d: flush %llu\n", i, tlb_quiescence[i].last_mandatory_flush);
		}
	}

	return quiescent;
}

int
chal_cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr,
                     vaddr_t order)
{
	unsigned long *    pte, cosframe, orig_v;
	struct cap_header *dest_pt_h;
	u32_t              flags;
	int                ret;

	if (unlikely(pt->lvl || (pt->refcnt_flags & CAP_MEM_FROZEN_FLAG))) return -EINVAL;
	dest_pt_h = captbl_lkup(ct, dest_pt);
	if (dest_pt_h->type != CAP_PGTBL) return -EINVAL;
	if (((struct cap_pgtbl *)dest_pt_h)->lvl) return -EINVAL;

	/* What is the order needed for this? */
	if (order != PGTBL_PAGEIDX_SHIFT) return -EINVAL;

	/* Only 4k pages are allowed for activation */
	pte = __chal_pgtbl_lkup((pgtbl_t)pt->pgtbl, frame_cap);
	if (!pte) return -EINVAL;
	orig_v = *pte;

	if (!(orig_v & CAV7_PGTBL_COSFRAME) || (orig_v & CAV7_PGTBL_COSKMEM)) return -EPERM;
	assert(!(orig_v & CAV7_PGTBL_QUIESCENCE));
	cosframe = orig_v & PGTBL_FRAME_MASK;
	flags    = CAV7_4K_USER_DEF;

	/* printk("cosframe %x dest pt %x vaddr %x flags %x\n", cosframe, ((struct cap_pgtbl *)dest_pt_h)->pgtbl, vaddr,
	 * flags); */
	ret = pgtbl_mapping_add(((struct cap_pgtbl *)dest_pt_h)->pgtbl, vaddr, cosframe, flags, order);
	return ret;
}

int
chal_pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl)
{
	struct cap_pgtbl *pt;
	int               ret;

	pt = (struct cap_pgtbl *)__cap_capactivate_pre(t, cap, capin, CAP_PGTBL, &ret);
	if (unlikely(!pt)) return ret;
	pt->pgtbl = pgtbl;

	pt->refcnt_flags = 1;
	pt->parent       = NULL; /* new cap has no parent. only copied cap has. */
	pt->lvl          = lvl;
	__cap_capactivate_post(&pt->h, CAP_PGTBL);

	return 0;
}

int
chal_pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, livenessid_t lid,
                      capid_t pgtbl_cap, capid_t cosframe_addr, const int root)
{
	struct cap_header *deact_header;
	struct cap_pgtbl * deact_cap, *parent;

	unsigned long l, old_v = 0, *pte = NULL;
	int           ret;

	deact_header = captbl_lkup(dest_ct_cap->captbl, capin);
	if (!deact_header || deact_header->type != CAP_PGTBL) cos_throw(err, -EINVAL);
	deact_cap = (struct cap_pgtbl *)deact_header;
	parent    = deact_cap->parent;

	l = deact_cap->refcnt_flags;
	assert(l & CAP_REFCNT_MAX);

	if ((l & CAP_REFCNT_MAX) != 1) {
		/* We need to deact children first! */
		cos_throw(err, -EINVAL);
	}

	if (parent == NULL) {
		if (!root) cos_throw(err, -EINVAL);
		/*
		 * Last reference to the captbl page. Require pgtbl
		 * and cos_frame cap to release the kmem page.
		 */
		ret = kmem_deact_pre(deact_header, t, pgtbl_cap, cosframe_addr, &pte, &old_v);
		if (ret) cos_throw(err, ret);
	} else {
		/* more reference exists. */
		if (root) cos_throw(err, -EINVAL);
		assert(!pgtbl_cap && !cosframe_addr);
	}

	if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, CAP_MEM_FROZEN_FLAG) != CAS_SUCCESS)
		cos_throw(err, -ECASFAIL);

	/*
	 * deactivation success. We should either release the
	 * page, or decrement parent cnt.
	 */
	if (parent == NULL) {
		/* move the kmem to COSFRAME */
		ret = kmem_deact_post(pte, old_v);
		if (ret) {
			cos_faa((int *)&deact_cap->refcnt_flags, 1);
			cos_throw(err, ret);
		}
	} else {
		cos_faa((int *)&parent->refcnt_flags, -1);
	}

	/* FIXME: this should be before the kmem_deact_post */
	ret = cap_capdeactivate(dest_ct_cap, capin, CAP_PGTBL, lid);
	if (ret) cos_throw(err, ret);

	return 0;
err:
	return ret;
}

int
chal_pgtbl_mapping_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags, u32_t order)
{
	int                ret = 0;
	struct ert_intern *pte;
	u32_t              orig_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* Only 4k pages are allowed for activation */
	if (order != PGTBL_PAGEIDX_SHIFT) return -EINVAL;

	/* look this page up */
	pte = (struct ert_intern *)__chal_pgtbl_lkup((pgtbl_t)pt->pgtbl, addr);

	if (!pte) return -ENOENT;
	orig_v = (u32_t)(pte->next);
	if (orig_v & CAV7_4K_PAGE_PRESENT) return -EEXIST;
	if (orig_v & CAV7_PGTBL_COSFRAME) return -EPERM;

	/* Quiescence check */
	ret = pgtbl_quie_check(orig_v);
	if (ret) return ret;

	/* ref cnt on the frame - always user frame. */
	ret = retypetbl_ref((void *)page, order);
	if (ret) return ret;

	ret = __pgtbl_update_leaf(pte, (void *)(page | flags), orig_v);
	/* restore the refcnt if necessary */
	if (ret) retypetbl_deref((void *)page, order);

	return ret;
}

int
chal_pgtbl_cosframe_add(pgtbl_t pt, u32_t addr, u32_t page, u32_t flags, u32_t order)
{
	struct ert_intern *pte;
	u32_t              orig_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & page) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/*
	 * Only 4k pages are allowed for adding currently. We will revive superpage
	 * support if we have time.
	 */
	if (order != PGTBL_PAGEIDX_SHIFT) return -EINVAL;

	pte = (struct ert_intern *)__chal_pgtbl_lkup((pgtbl_t)pt->pgtbl, addr);

	if (!pte) return -ENOENT;
	orig_v = (u32_t)(pte->next);
	assert(orig_v == 0);

	return __pgtbl_update_leaf(pte, (void *)(page | flags), 0);
}

/* This function updates flags of an existing mapping. */
int
chal_pgtbl_mapping_mod(pgtbl_t pt, u32_t addr, u32_t flags, u32_t *prevflags)
{
	/* Not used for now. TODO: add retypetbl_ref / _deref */

	struct ert_intern *pte;
	u32_t              orig_v, accum = 0;

	assert(pt && prevflags);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	assert((PGTBL_FRAME_MASK & flags) == 0);

	/* get the pte */
	pte = (struct ert_intern *)__chal_pgtbl_lkup(pt, addr);
	if (__pgtbl_isnull(pte, 0, 0)) return -ENOENT;

	orig_v = (u32_t)(pte->next);
	/**
	 * accum contains flags from pgd as well, so don't use it to
	 * get prevflags.
	 */
	*prevflags = orig_v & PGTBL_FLAG_MASK;

	/* and update the flags. */
	return __pgtbl_update_leaf(pte, (void *)((orig_v & PGTBL_FRAME_MASK) | ((u32_t)flags & PGTBL_FLAG_MASK)),
	                           orig_v);
}

/**
 * When we remove a mapping, we need to link the vas to a liv_id,
 * which tracks quiescence for us.
 */
int
chal_pgtbl_mapping_del(pgtbl_t pt, u32_t addr, u32_t liv_id)
{
	int                ret;
	struct ert_intern *pte;
	unsigned long      orig_v, accum = 0;
	vaddr_t            order;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* In pgtbl, we have only 20bits for liv id. */
	if (unlikely(liv_id >= (1 << (32 - PGTBL_PAGEIDX_SHIFT)))) return -EINVAL;

	/* Liveness tracking of the unmapping VAS. */
	ret = ltbl_timestamp_update(liv_id);
	if (unlikely(ret)) goto done;

	/* Get the PGD to see if we are deleting a superpage */
	pte    = (struct ert_intern *)__chal_pgtbl_lkup(pt, addr);
	orig_v = (u32_t)(pte->next);
	if (!(orig_v & CAV7_4K_PAGE_PRESENT)) return -EEXIST;
	if (orig_v & CAV7_PGTBL_COSFRAME) return -EPERM;

	order = PAGE_ORDER;
	ret   = __pgtbl_update_leaf(pte, (void *)((liv_id << PGTBL_PAGEIDX_SHIFT) | CAV7_PGTBL_QUIESCENCE), orig_v);
	if (ret) cos_throw(done, ret);

	/* decrement ref cnt on the frame. */
	ret = retypetbl_deref((void *)(orig_v & PGTBL_FRAME_MASK), order);
	if (ret) cos_throw(done, ret);

done:
	return ret;
}

/**
 * NOTE: This just removes the mapping. NO liveness tracking! TLB
 * flush should be taken care of separately (and carefully).
 */
int
chal_pgtbl_mapping_del_direct(pgtbl_t pt, u32_t addr)
{
	unsigned long accum = 0, *pte = NULL;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);
	pte  = __chal_pgtbl_lkup(pt, addr);
	*pte = 0;
	return 0;
}

int
chal_pgtbl_mapping_scan(struct cap_pgtbl *pt)
{
	unsigned int i, pte, *page;
	livenessid_t lid;
	u64_t        past_ts;

	/**
	 * This scans the leaf level of the pgtbl and verifies
	 * quiescence.
	 */
	if (pt->lvl != PGTBL_DEPTH - 1) return -EINVAL;

	page = (unsigned int *)(pt->pgtbl);
	assert(page);

	for (i = 0; i < PAGE_SIZE / sizeof(int *); i++) {
		pte = *(page + i);
		if (pte & CAV7_4K_PAGE_PRESENT || pte & CAV7_PGTBL_COSFRAME) return -EINVAL;

		if (pte & CAV7_PGTBL_QUIESCENCE) {
			lid = pte >> PGTBL_PAGEIDX_SHIFT;

			if (ltbl_get_timestamp(lid, &past_ts)) return -EFAULT;
			if (!tlb_quiescence_check(past_ts)) return -EQUIESCENCE;
		}
	}

	return 0;
}

/* Perform lookup to a certain level */
void *
chal_pgtbl_lkup_lvl(pgtbl_t pt, u32_t addr, u32_t *flags, u32_t start_lvl, u32_t end_lvl)
{
	/* We return level 1 in all cases */
	unsigned long *next;

	if (addr >= 0x40000000) return 0;

	next = &(((unsigned long *)pt)[addr >> PGTBL_PGTIDX_SHIFT]);
	/*
	        if ((((unsigned long)next) & CAV7_1M_PGDIR_PRESENT) == 0)
	                return 0;
	*/
	return next;
}

int
chal_pgtbl_ispresent(u32_t flags)
{
	return flags & (CAV7_4K_PAGE_PRESENT | CAV7_PGTBL_COSFRAME);
}

unsigned long *
chal_pgtbl_lkup(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	unsigned long *pte;
	/* We return level 2 only in all lookups */
	pte    = __chal_pgtbl_lkup(pt, addr);
	*flags = chal_pgtbl_flag_nat2cos_4K(*pte);
	if (!pgtbl_ispresent(*flags)) return NULL;

	return (unsigned long *)chal_pa2va(CAV7_4K_PAGE_ADDR(*pte));
}

unsigned long *
chal_pgtbl_lkup_pte(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	unsigned long *pte;

	/* We return level 2 only in all lookups */
	pte    = __chal_pgtbl_lkup(pt, addr);
	*flags = chal_pgtbl_flag_nat2cos_4K(*pte);
	if (!pgtbl_ispresent(*flags)) return NULL;

	return pte;
}

unsigned long *
chal_pgtbl_lkup_pgd(pgtbl_t pt, u32_t addr, u32_t *flags)
{
	/* We return level 1 in all cases */
	unsigned long *next;

	if (addr >= 0x40000000) return 0;

	next   = &(((unsigned long *)pt)[addr >> PGTBL_PGTIDX_SHIFT]);
	*flags = ((unsigned long)pt) & (0xFFFFFFFF << PGTBL_PGTIDX_SHIFT);

	return next;
}

int
chal_pgtbl_get_cosframe(pgtbl_t pt, vaddr_t frame_addr, paddr_t *cosframe, vaddr_t *order)
{
	u32_t          flags;
	unsigned long *pte;
	paddr_t        v;

	/* What is the order of this cosframe? */
	pte = pgtbl_lkup_pte(pt, frame_addr, &flags);
	if (!pte) return -EINVAL;

	v      = *pte;
	*order = PAGE_ORDER;

	if (!(v & CAV7_PGTBL_COSFRAME)) return -EINVAL;

	*cosframe = v & PGTBL_FRAME_MASK;

	return 0;
}

pgtbl_t
chal_pgtbl_create(void *page, void *curr_pgtbl)
{
	pgtbl_t ret = (pgtbl_t)(page);

	memset(page, 0, PAGE_SIZE);
	/* Copying the kernel part of the pgd. */
	memcpy(page + KERNEL_PGD_REGION_OFFSET, (void *)chal_pa2va((paddr_t)curr_pgtbl) + KERNEL_PGD_REGION_OFFSET,
	       KERNEL_PGD_REGION_SIZE);

	return ret;
}

int
chal_pgtbl_quie_check(u32_t orig_v)
{
	livenessid_t lid;
	u64_t        ts;

	if (orig_v & CAV7_PGTBL_QUIESCENCE) {
		lid = orig_v >> PGTBL_PAGEIDX_SHIFT;
		/*
		 * An unmap happened at this vaddr before. We need to
		 * make sure that all cores have done tlb flush before
		 * creating new mapping.
		 */
		assert(lid < LTBL_ENTS);

		if (ltbl_get_timestamp(lid, &ts)) return -EFAULT;
		if (!tlb_quiescence_check(ts)) {
			printk("kern tsc %llu, lid %d, last flush %llu\n", ts, lid,
			       tlb_quiescence[get_cpuid()].last_periodic_flush);
			return -EQUIESCENCE;
		}
	}

	return 0;
}

void
chal_pgtbl_init_pte(void *pte)
{
	int            i;
	unsigned long *vals = pte;
	for (i = 0; i < (1 << 8 /* (PGTBL_ENTRY_ORDER) */); i++) vals[i] = 0;
}

extern const int order2pos[];
#define POS(order) (order2pos[order])
#define LVL(order) (1 - POS(order))
int
chal_pgtbl_pgtblactivate(struct captbl *ct, capid_t cap, capid_t pt_entry, capid_t pgtbl_cap, vaddr_t kmem_cap,
                         capid_t pgtbl_order)
{
	pgtbl_t        new_pt, curr_pt;
	vaddr_t        kmem_addr = 0;
	unsigned long *pte       = NULL;
	int            ret;
	unsigned long  pgtbl_lvl = 3;

	ret = cap_kmem_activate(ct, pgtbl_cap, kmem_cap, (unsigned long *)&kmem_addr, &pte);
	if (unlikely(ret)) return ret;
	assert(kmem_addr && pte);

	/* Convert order to level here */
	pgtbl_lvl = LVL(pgtbl_order);

	if (pgtbl_lvl == 0) {
		/* PGD */
		struct cap_pgtbl *cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, pgtbl_cap);
		if (!CAP_TYPECHK(cap_pt, CAP_PGTBL)) return -EINVAL;

		curr_pt = cap_pt->pgtbl;
		assert(curr_pt);

		new_pt = pgtbl_create((void *)kmem_addr, curr_pt);
		ret    = pgtbl_activate(ct, cap, pt_entry, new_pt, 0);
	} else if (pgtbl_lvl == 1) {
		/* PTE */
		pgtbl_init_pte((void *)kmem_addr);
		ret = pgtbl_activate(ct, cap, pt_entry, (pgtbl_t)kmem_addr, 1);
	} else {
		/* Not supported yet. */
		printk("cos: warning - PGTBL level greater than 2 not supported yet. \n");
		ret = -1;
	}

	if (ret) kmem_unalloc(pte);
	return ret;
}

int
chal_pgtbl_cpy(struct captbl *t, capid_t cap_to, capid_t capin_to, struct cap_pgtbl *ctfrom, capid_t capin_from,
               cap_t cap_type, vaddr_t order)
{
	struct cap_header *ctto;
	unsigned long *    f, old_v;
	u32_t              flags;

	ctto = captbl_lkup(t, cap_to);
	if (unlikely(!ctto)) return -ENOENT;
	if (unlikely(ctto->type != cap_type)) return -EINVAL;
	if (unlikely(((struct cap_pgtbl *)ctto)->refcnt_flags & CAP_MEM_FROZEN_FLAG)) return -EINVAL;

	/*
	 * See what kind of delegation we are doing. There are 4 kinds of delegations:
	 * 1. Superpage -> Smallpage [order = 12]
	 * 2. Superpage -> Superpage [order = 22]
	 * 3. Smallpage -> Smallpage [order = 12]
	 * 4. Smallpage -> Superpage [prohibited]
	 */
	/* How big is the current page? */
	// printk("pgtbl %x capin_from %x order %d\n",((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, order);
	//	f = pgtbl_lkup_pgd(((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, &flags);
	//	if (!f) return -ENOENT;
	//	old_v = *f;
	//
	//	if (chal_pgtbl_flag_exist(old_v, PGTBL_SUPER)) {
	//		flags = chal_pgtbl_flag(old_v);
	//		if (order != SUPER_PAGE_ORDER) {
	//			/* We need to pick a subpage */
	//			old_v += EXTRACT_SUB_PAGE(capin_from);
	//			flags &= (~PGTBL_SUPER);
	//		} else {
	//			/* If we do superpage to superpage delegation, both addresses must be aligned to 4MB
	// boundary */ 			if ((EXTRACT_SUB_PAGE(capin_from) != 0) || (EXTRACT_SUB_PAGE(capin_to) != 0))
	// return -EINVAL;
	//		}
	//	} else {
	if (order != PAGE_ORDER) return -EINVAL;
	f     = pgtbl_lkup_pte(((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, &flags);
	flags = chal_pgtbl_flag_cos2nat_4K(flags);
	if (!f) return -ENOENT;
	old_v = *f;
	//	}

	//	printk("%d old_v %x\n",__LINE__, old_v);
	/* Cannot copy frame, or kernel entry. */
	if (chal_pgtbl_flag_exist(old_v, PGTBL_COSFRAME) || !chal_pgtbl_flag_exist(old_v, PGTBL_USER)) return -EPERM;
	//	printk("%d\n",__LINE__);
	return pgtbl_mapping_add(((struct cap_pgtbl *)ctto)->pgtbl, capin_to, old_v & PGTBL_FRAME_MASK, flags, order);
}

/* FIXME: we need to ensure TLB quiescence for pgtbl cons/decons!
 * ct - main table capability
 * ctsub - sub table capability
 * expandid - address to place the subtable
 * depth - the depth level of the subtable
 */
int
chal_pgtbl_cons(struct cap_captbl *ct, struct cap_captbl *ctsub, capid_t expandid, unsigned long depth)
{
	u32_t          flags = 0, old_pte, new_pte, old_v, refcnt_flags;
	unsigned long *intern;
	int            ret = 0;

	intern = pgtbl_lkup_lvl(((struct cap_pgtbl *)ct)->pgtbl, expandid, &flags, ct->lvl, depth);

	if (!intern) return -ENOENT;
	old_pte = *intern;

	if (pgtbl_ispresent(old_pte)) return -EPERM;

	old_v = refcnt_flags = ((struct cap_pgtbl *)ctsub)->refcnt_flags;
	if (refcnt_flags & CAP_MEM_FROZEN_FLAG) return -EINVAL;
	if ((refcnt_flags & CAP_REFCNT_MAX) == CAP_REFCNT_MAX) return -EOVERFLOW;

	refcnt_flags++;
	ret = cos_cas((unsigned long *)&(((struct cap_pgtbl *)ctsub)->refcnt_flags), old_v, refcnt_flags);
	if (ret != CAS_SUCCESS) return -ECASFAIL;

	new_pte = (u32_t)chal_va2pa((void *)((unsigned long)(((struct cap_pgtbl *)ctsub)->pgtbl) & PGTBL_FRAME_MASK))
	          | CAV7_1M_INTERN_DEF;

	/* printk("expandid %x, depth %x, new_pte %x\n", expandid, depth, new_pte); */

	ret = cos_cas(intern, old_pte, new_pte);
	if (ret != CAS_SUCCESS) {
		/* decrement to restore the refcnt on failure. */
		cos_faa((int *)&(((struct cap_pgtbl *)ctsub)->refcnt_flags), -1);
		return -ECASFAIL;
	} else {
		ret = 0;
	}

	return ret;
}

int
chal_pgtbl_decons(struct cap_header *head, struct cap_header *sub, capid_t pruneid, unsigned long lvl)
{
	unsigned long *intern, old_v;

	struct cap_pgtbl *pt = (struct cap_pgtbl *)head;
	u32_t             flags;
	if (lvl <= pt->lvl) return -EINVAL;
	intern = pgtbl_lkup_lvl(pt->pgtbl, pruneid, &flags, pt->lvl, lvl);

	if (!intern) return -ENOENT;
	old_v = *intern;

	/* Is this really a pte or we are trying to decons on superpages - prohibited? */
	if (old_v & CAV7_1M_PAGE_PRESENT) return -EINVAL;
	if (old_v == 0) return 0; /* FIXME: old comment - return an error here? */
	/* commit; note that 0 is "no entry" in both pgtbl and captbl */
	if (cos_cas(intern, old_v, 0) != CAS_SUCCESS) return -ECASFAIL;

	/* decrement the refcnt */
	pt = (struct cap_pgtbl *)sub;
	u32_t l;
	l = pt->refcnt_flags;
	if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
	cos_faa((int *)&(pt->refcnt_flags), -1);

	return 0;
}

int
chal_pgtbl_introspect(struct cap_header *ch, vaddr_t addr)
{
	unsigned long *pte;
	u32_t          flags;
	int            ret = 0;
	/* Is this a pte or a pgd? */
	pte = pgtbl_lkup_pgd(((struct cap_pgtbl *)ch)->pgtbl, addr, &flags);
	if (pte) {
		if (chal_pgtbl_flag_exist(*pte, PGTBL_SUPER)) {
			ret = *pte;
		} else {
			pte = pgtbl_lkup_pte(((struct cap_pgtbl *)ch)->pgtbl, addr, &flags);
			if (pte) ret = *pte;
		}
	}

	return ret;
}

int
chal_pgtbl_deact_pre(struct cap_header *ch, u32_t pa)
{
	struct cap_pgtbl *deact_cap = (struct cap_pgtbl *)ch;
	void *            page      = deact_cap->pgtbl;
	u32_t             l         = deact_cap->refcnt_flags;
	u64_t             curr;
	int               ret;

	if (chal_pa2va((paddr_t)pa) != page) return -EINVAL;

	/* Require freeze memory and wait for quiescence
	 * first! */
	if (!(l & CAP_MEM_FROZEN_FLAG)) return -EQUIESCENCE;

	/* Quiescence check! */
	if (deact_cap->lvl == 0) {
		/* top level has tlb quiescence period. */
		if (!tlb_quiescence_check(deact_cap->frozen_ts)) return -EQUIESCENCE;
	} else {
		/* other levels have kernel quiescence
		 * period. (but the mapping scan will ensure
		 * tlb quiescence implicitly). */
		rdtscll(curr);
		if (!QUIESCENCE_CHECK(curr, deact_cap->frozen_ts, KERN_QUIESCENCE_CYCLES)) return -EQUIESCENCE;
	}

	/* set the scan flag to avoid concurrent scanning. */
	if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, l | CAP_MEM_SCAN_FLAG) != CAS_SUCCESS)
		return -ECASFAIL;

	if (deact_cap->lvl == 0) {
		/* PGD: only scan user mapping. */
		ret = kmem_page_scan(page, PAGE_SIZE - KERNEL_PGD_REGION_SIZE);
	} else if (deact_cap->lvl == PGTBL_DEPTH - 1) {
		/* Leaf level, scan mapping. */
		ret = pgtbl_mapping_scan(deact_cap);
	} else {
		/* don't have this with 2-level pgtbl. */
		ret = kmem_page_scan(page, PAGE_SIZE);
	}

	if (ret) {
		/* unset scan and frozen bits. */
		cos_cas((unsigned long *)&deact_cap->refcnt_flags, l | CAP_MEM_SCAN_FLAG,
		        l & ~(CAP_MEM_FROZEN_FLAG | CAP_MEM_SCAN_FLAG));
		return ret;
	}
	cos_cas((unsigned long *)&deact_cap->refcnt_flags, l | CAP_MEM_SCAN_FLAG, l);

	return 0;
}
extern void __cos_cav7_ttbr0_set(paddr_t p);
extern void __cos_cav7_tlbiall_set(int);

void
chal_pgtbl_update(pgtbl_t pt)
{
	/* Set TTBR0 to this. Rememberto VA2PA (va is passed in), and add the "0x4A" flag to it */
	// printk("set pgtbl %x\n", pt);
	/* HACK OF ALL HACKS - STOP PMU ON SWITCHING TO OTHER COMPONENT - FAILURE. GOOD. */
	//	if(pt == 0x98003000 /* 80010000 */)
	//		__cos_cav7_pmcntenset_set(0x80000000UL);
	//	else
	//		__cos_cav7_pmcntenset_set(0x80000000UL);

	//	if(pt ==  0x98003000/* 0x80001000 */)
	//		__cos_cav7_pmcntenset_set(0x80000001UL);
	//	else
	//		__cos_cav7_pmcntenclr_set(0x00000001UL);

	__cos_cav7_ttbr0_set(chal_va2pa(pt) | 0x4A);
	__cos_cav7_tlbiall_set(0);
}
