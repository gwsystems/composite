#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/pgtbl.h"
#include "include/cap_ops.h"
#include "include/liveness_tbl.h"
#include "include/retype_tbl.h"

int
cap_kmem_freeze(struct captbl *t, capid_t target_cap)
{
	struct cap_header *ch;
	u32_t l;
	int ret;
	
	ch = captbl_lkup(t, target_cap);

	/* Only memory for captbl and pgtbl needs to be frozen before
	 * deactivation. */
	if (ch->type == CAP_CAPTBL) {
		struct cap_captbl *ct = (struct cap_captbl *)ch;
 		l = ct->refcnt_flags;
		if ((l & CAP_REFCNT_MAX) > 1 || l & CAP_MEM_FROZEN_FLAG) return -EINVAL;

		ret = cos_cas((unsigned long *)&ct->refcnt_flags, l, l | CAP_MEM_FROZEN_FLAG);
	} else if (ch->type == CAP_PGTBL) {
		struct cap_pgtbl *pt = (struct cap_pgtbl *)ch;
		l = pt->refcnt_flags;
		if ((l & CAP_REFCNT_MAX) > 1 || l & CAP_MEM_FROZEN_FLAG) return -EINVAL;

		ret = cos_cas((unsigned long *)&pt->refcnt_flags, l, l | CAP_MEM_FROZEN_FLAG);
	} else {
		return -EINVAL;
	}
	
	return 0;
}

static int 
kmem_page_scan(void *obj_vaddr) 
{
	/* For non-leaf level captbl / pgtbl. entries are all pointers
	 * in these cases. */
	int i;
	void *addr = obj_vaddr;
			
	for (i = 0; i < PAGE_SIZE / sizeof(void *); i++) {
		if (*(unsigned long *)addr != 0) return -EINVAL;
		addr++;
	}

	return 0;
}

/* The deact_pre / _post are used by kernel object deactivation:
 * cap_captbl, cap_pgtbl and thd. */
int 
kmem_deact_pre(struct cap_header *ch, struct captbl *ct, capid_t pgtbl_cap, 
	       capid_t cosframe_addr, unsigned long **p_pte, unsigned long *v)
{
	struct cap_pgtbl *cap_pt;
	u32_t flags, old_v, pa;
	int ret;

	assert(ct && ch);
	if (!pgtbl_cap || !cosframe_addr)
		cos_throw(err, -EINVAL);

	cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, pgtbl_cap);
	if (cap_pt->h.type != CAP_PGTBL) cos_throw(err, -EINVAL);

	/* get the pte to the cos frame. */
	*p_pte = pgtbl_lkup_pte(cap_pt->pgtbl, cosframe_addr, &flags);
	old_v = *v = **p_pte;

	pa = old_v & PGTBL_FRAME_MASK;
	if (!(old_v & PGTBL_COSKMEM) || (old_v & PGTBL_QUIESCENCE)) cos_throw(err, -EINVAL);
	assert(old_v & PGTBL_COSFRAME);

	/* Scan the page to make sure there's nothing left. */
	if (ch->type == CAP_CAPTBL) {
		struct cap_captbl *ct = (struct cap_captbl *)ch;
		void *page = ct->captbl;

		if ((void *)chal_pa2va((void *)pa) != page) cos_throw(err, -EINVAL);
		
		if (ct->lvl < CAPTBL_DEPTH - 1) {
			ret = kmem_page_scan(page);
		} else {
			ret = captbl_kmem_scan(page);
		}

		if (ret) cos_throw(err, ret);
	} else if (ch->type == CAP_PGTBL) {
		struct cap_pgtbl *pt = (struct cap_pgtbl *)ch;
		void *page = pt->pgtbl;

		if ((void *)chal_pa2va((void *)pa) != page) cos_throw(err, -EINVAL);

		if (pt->lvl < PGTBL_DEPTH - 1) {
			ret = kmem_page_scan(page);
		} else {
			ret = pgtbl_mapping_sacn(page);
		}

		if (ret) cos_throw(err, ret);
	} else {
		/* currently only captbl and pgtbl pages need to be
		 * scanned before deactivation. */
		struct cap_thd *tc = (struct cap_pgtbl *)ch;
		if ((void *)chal_pa2va((void *)pa) != (void *)(tc->t)) cos_throw(err, -EINVAL);

		assert(ch->type == CAP_THD);
	}

	return 0;
err:
	return ret;
}

/* Updates the pte, deref the frame. */
int 
kmem_deact_post(unsigned long *pte, unsigned long old_v)
{
	int ret;
	u32_t new_v;

	/* Unset coskmem bit. Release the kmem frame. */
	new_v = old_v & ~PGTBL_COSKMEM;
	if (cos_cas(pte, old_v, new_v) != CAS_SUCCESS) cos_throw(err, -ECASFAIL);

	ret = retypetbl_deref((void *)(old_v & PGTBL_FRAME_MASK));
	if (ret) {
		/* FIXME: handle this case? */
		cos_cas(pte, new_v, old_v);
		cos_throw(err, -ECASFAIL);
	}

	return 0;
err:
	return ret;
}

int
pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr)
{
	int ret;
	struct ert_intern *pte;
	u32_t orig_v, new_v, accum = 0;
	
	assert(pt);
	assert((PGTBL_FLAG_MASK & addr) == 0);

	/* get the pte */
	pte = (struct ert_intern *)__pgtbl_lkupan((pgtbl_t)((u32_t)pt|PGTBL_PRESENT), 
						  addr >> PGTBL_PAGEIDX_SHIFT, PGTBL_DEPTH, &accum);
	if (unlikely(__pgtbl_isnull(pte, 0, 0))) return -ENOENT;

	orig_v = (u32_t)(pte->next);
	if (unlikely(!(orig_v & PGTBL_COSFRAME))) return -EINVAL; /* can't activate non-frames */

	if (orig_v & PGTBL_QUIESCENCE) {
		u32_t frame;

		ret = get_quiescent_frame(orig_v, &frame);
		if (ret) return ret;

		*kern_addr = (unsigned long)chal_pa2va((void *)(frame));
		new_v = frame | (orig_v & PGTBL_FLAG_MASK) | PGTBL_COSKMEM;
	} else {
		if (unlikely(orig_v & PGTBL_COSKMEM)) return -EEXIST; /* can't re-activate kmem frames */

		*kern_addr = (unsigned long)chal_pa2va((void *)(orig_v & PGTBL_FRAME_MASK));
		new_v = orig_v | PGTBL_COSKMEM;
	}

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

	/* Now we can remove the kmem frame stored in the poly of the
	 * ltbl entry. */
	if (orig_v & PGTBL_QUIESCENCE) 
		ltbl_poly_clear(orig_v >> PGTBL_PAGEIDX_SHIFT);

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

	return quiescent;
}

int
cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr)
{
	unsigned long *pte, cosframe, orig_v;
	struct cap_header *dest_pt_h;
	u32_t flags;
	int ret;

	if (unlikely(pt->lvl || (pt->refcnt_flags & CAP_MEM_FROZEN_FLAG))) return -EINVAL;

	dest_pt_h = captbl_lkup(ct, dest_pt);
	if (dest_pt_h->type != CAP_PGTBL) return -EINVAL;

	pte = pgtbl_lkup_pte(pt->pgtbl, frame_cap, &flags);
	if (!pte) return -EINVAL;
	orig_v = *pte;

	if (!(orig_v & PGTBL_COSFRAME) || (orig_v & PGTBL_COSKMEM)) return -EPERM;

	if (orig_v & PGTBL_QUIESCENCE) {
		u32_t frame;
		/* This frame was used as kmem, and was waiting for quiescence. */
		ret = get_quiescent_frame(orig_v, &frame);
		if (ret) return ret;
		assert(frame);
		assert(!(frame & PGTBL_FLAG_MASK));
		cosframe = frame;
	} else {
		cosframe = orig_v & PGTBL_FRAME_MASK;
	}

	ret = pgtbl_mapping_add(((struct cap_pgtbl *)dest_pt_h)->pgtbl, vaddr, 
				cosframe, PGTBL_USER_DEF);
	if (!ret && (orig_v & PGTBL_QUIESCENCE)) {
		/* Now we can clear the poly. */
		ltbl_poly_clear(orig_v >> PGTBL_PAGEIDX_SHIFT);
	}

	return ret;
}

int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl)
{
	struct cap_pgtbl *pt;
	int ret;
	
	pt = (struct cap_pgtbl *)__cap_capactivate_pre(t, cap, capin, CAP_PGTBL, &ret);
	if (!unlikely(pt)) return ret;
	pt->pgtbl  = pgtbl;

	pt->refcnt_flags = 1;
	pt->parent = NULL; /* new cap has no parent. only copied cap has. */
	pt->lvl    = lvl;
	__cap_capactivate_post(&pt->h, CAP_PGTBL);

	return 0;
}

int
pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, 
		 livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr)
{ 
	struct cap_header *deact_header;
	struct cap_pgtbl *deact_cap, *parent;

	unsigned long l, old_v = 0, *pte = NULL;
	int ret;

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
		/* Last reference to the captbl page. Require pgtbl
		 * and cos_frame cap to release the kmem page. */
		ret = kmem_deact_pre(deact_header, t, pgtbl_cap, 
				     (void *)(deact_cap->pgtbl), &pte, &old_v);
		if (ret) cos_throw(err, ret);
	} else {
		/* more reference exists. just sanity
		 * checks. */
		if (pgtbl_cap || cosframe_addr || kmem_lid) {
			/* we pass in the pgtbl cap and frame addr,
			 * but ref_cnt is > 1. We'll ignore the two
			 * parameters as we won't be able to release
			 * the memory. */
			printk("cos: deactivating pgtbl but not able to release kmem page (%p) yet (ref_cnt %d).\n", 
			       (void *)cosframe_addr, (int)l);
		}
	}

	ret = cap_capdeactivate(dest_ct_cap, capin, CAP_PGTBL, lid);
	if (ret) cos_throw(err, ret);

	if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, 0) != CAS_SUCCESS) cos_throw(err, -ECASFAIL);

	/* deactivation success. We should either release the
	 * page, or decrement parent cnt. */
	if (parent == NULL) { 
		/* move the kmem to COSFRAME */
		ret = kmem_deact_post(pte, old_v, kmem_lid);
		if (ret) {
			cos_faa((int *)&deact_cap->refcnt_flags, 1);
			cos_throw(err, ret);
		}
	} else {
		cos_faa(&parent->refcnt_flags, -1);
	}

	return 0;
err:
	return ret;
}
