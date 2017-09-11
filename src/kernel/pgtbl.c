#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/pgtbl.h"
#include "include/cap_ops.h"
#include "include/liveness_tbl.h"
#include "include/retype_tbl.h"

int
pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret)
{
	struct ert_intern *pte;
	u32_t              orig_v, new_v, accum = 0;

	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* get the pte */
	pte = (struct ert_intern *)__pgtbl_lkupan((pgtbl_t)((u32_t)pt | PGTBL_PRESENT), addr >> PGTBL_PAGEIDX_SHIFT,
	                                          PGTBL_DEPTH, &accum);
	if (unlikely(!pte)) return -ENOENT;
	if (unlikely(__pgtbl_isnull(pte, 0, 0))) return -ENOENT;

	orig_v = (u32_t)(pte->next);
	if (unlikely(!(orig_v & PGTBL_COSFRAME))) return -EINVAL; /* can't activate non-frames */
	if (unlikely(orig_v & PGTBL_COSKMEM)) return -EEXIST;     /* can't re-activate kmem frames */
	assert(!(orig_v & PGTBL_QUIESCENCE));

	*kern_addr = (unsigned long)chal_pa2va((paddr_t)(orig_v & PGTBL_FRAME_MASK));
	new_v      = orig_v | PGTBL_COSKMEM;

	/* pa2va (value in *kern_addr) will return NULL if the page is
	 * not kernel accessible */
	if (unlikely(!*kern_addr)) return -EINVAL; /* cannot retype a non-kernel accessible page */
	if (unlikely(retypetbl_kern_ref((void *)(new_v & PGTBL_FRAME_MASK)))) return -EFAULT;

	/* We keep the cos_frame entry, but mark it as COSKMEM so that
	 * we won't use it for other kernel objects. */
	if (unlikely(cos_cas((unsigned long *)pte, orig_v, new_v) != CAS_SUCCESS)) {
		/* restore the ref cnt. */
		retypetbl_deref((void *)(orig_v & PGTBL_FRAME_MASK));
		return -ECASFAIL;
	}
	/* Return the pte ptr, so that we can release the page if the
	 * kobj activation failed later. */
	*pte_ret = (unsigned long *)pte;

	return 0;
}

/* Return 1 if quiescent past since input timestamp. 0 if not. */
int
tlb_quiescence_check(u64_t timestamp)
{
	int i, quiescent = 1;

	/* Did timer interrupt (which does tlb flush
	 * periodically) happen after unmap? The periodic
	 * flush happens on all cpus, thus only need to check
	 * the time stamp of the current core for that case
	 * (assuming consistent time stamp counters). */
	if (timestamp > tlb_quiescence[get_cpuid()].last_periodic_flush) {
		/* If no periodic flush done yet, did the
		 * mandatory flush happen on all cores? */
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
cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr)
{
	unsigned long *    pte, cosframe, orig_v;
	struct cap_header *dest_pt_h;
	u32_t              flags;
	int                ret;

	if (unlikely(pt->lvl || (pt->refcnt_flags & CAP_MEM_FROZEN_FLAG))) return -EINVAL;

	dest_pt_h = captbl_lkup(ct, dest_pt);
	if (dest_pt_h->type != CAP_PGTBL) return -EINVAL;
	if (((struct cap_pgtbl *)dest_pt_h)->lvl) return -EINVAL;

	pte = pgtbl_lkup_pte(pt->pgtbl, frame_cap, &flags);
	if (!pte) return -EINVAL;
	orig_v = *pte;

	if (!(orig_v & PGTBL_COSFRAME) || (orig_v & PGTBL_COSKMEM)) return -EPERM;

	assert(!(orig_v & PGTBL_QUIESCENCE));
	cosframe = orig_v & PGTBL_FRAME_MASK;

	ret = pgtbl_mapping_add(((struct cap_pgtbl *)dest_pt_h)->pgtbl, vaddr, cosframe, PGTBL_USER_DEF);

	return ret;
}

int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl)
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
pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, livenessid_t lid,
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
		/* Last reference to the captbl page. Require pgtbl
		 * and cos_frame cap to release the kmem page. */
		ret = kmem_deact_pre(deact_header, t, pgtbl_cap, cosframe_addr, &pte, &old_v);
		if (ret) cos_throw(err, ret);
	} else {
		/* more reference exists. */
		if (root) cos_throw(err, -EINVAL);
		assert(!pgtbl_cap && !cosframe_addr);
	}

	if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, CAP_MEM_FROZEN_FLAG) != CAS_SUCCESS)
		cos_throw(err, -ECASFAIL);

	/* deactivation success. We should either release the
	 * page, or decrement parent cnt. */
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
