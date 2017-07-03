#include "include/chal/shared/cos_types.h"
#include "include/captbl.h"
#include "include/pgtbl.h"
#include "include/cap_ops.h"
#include "include/liveness_tbl.h"
#include "include/retype_tbl.h"

/* We are activatng some memory as kernel memory. every page must be 4kB */
int
pgtbl_kmem_act(pgtbl_t pt, u32_t addr, unsigned long *kern_addr, unsigned long **pte_ret)
{
	u32_t* pte;
	u32_t size_order;
	u32_t old_flags;

	assert(pt);

	/* get the pte */
	pte=pgtbl_lkup_leaf(pt, addr, &size_order, &old_flags);

	if (unlikely(!pte))  return -ENOENT;
	if (unlikely(_pgtbl_isnull((u32_t)pte))) return -ENOENT;

	if (unlikely(!(*pte & PGTBL_COSFRAME))) return -EINVAL; /* can't activate non-frames */
	if (unlikely(*pte & PGTBL_COSKMEM)) return -EEXIST; /* can't re-activate kmem frames */

	*kern_addr = addr;
	*pte = *pte | PGTBL_COSKMEM;

	if (unlikely(retypetbl_kern_ref((void *)(*pte & PGTBL_FRAME_MASK)))) return -EFAULT;

	/* PRY:We do not need CAS here, deleted */
	/* Return the pte ptr, so that we can release the page if the
	 * kobj activation failed later. */
	*pte_ret = (unsigned long *)pte;

	return 0;
}

/* Return 1 if quiescent past since input timestamp. 0 if not. */
int
tlb_quiescence_check(u64_t timestamp)
{
	/* PRY: here we directly return 1, no need to check quiescence */
	return 1;
}

int
cap_memactivate(struct captbl *ct, struct cap_pgtbl *pt, capid_t frame_cap, capid_t dest_pt, vaddr_t vaddr)
{
	/* Trying to map something to userspace */
	u32_t* pte;
	u32_t size_order;
	u32_t old_flags;
	unsigned long cosframe, orig_v;
	struct cap_header *dest_pt_h;
	int ret;

	/* Composite enforced checking */
	if (unlikely(pt->refcnt_flags & CAP_MEM_FROZEN_FLAG)) return -EINVAL;

	/* Get the destination page table, and see if this is really a page table */
	dest_pt_h = captbl_lkup(ct, dest_pt);
	if (dest_pt_h->type != CAP_PGTBL) return -EINVAL;

	/* Is this mapping allowed? Is this a cosframe, or ? */
	pte=pgtbl_lkup_leaf(&(pt->pgtbl), frame_cap, &size_order, &old_flags);

	if (!pte) return -EINVAL;
	/* If what we are mapping is not a frame(that means the page is nonexistent), or that page contains
	 * kernel objects, never map that to user space */
	if (!((*pte) & PGTBL_COSFRAME) || ((*pte) & PGTBL_COSKMEM)) return -EPERM;
	/* Clean all other flags */
	*pte &= PGTBL_FRAME_MASK;
	/* We add the mapping */
	ret = pgtbl_mapping_add(&(((struct cap_pgtbl *)dest_pt_h)->pgtbl), vaddr,*pte, PGTBL_USER_DEF);

	return ret;
}

int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, u32_t type, u32_t start_addr, u32_t size_order, u32_t num_order)
{
	struct cap_pgtbl *pt;
	int count;
	int ret;
	
	/* Activate the page table as desired */
	pt = (struct cap_pgtbl *)__cap_capactivate_pre(t, cap, capin, CAP_PGTBL, &ret);
	if (unlikely(!pt)) return ret;

	/* See if this is the top level. If it is not, set the field to 0 */
	if(type!=0) {
		pt->pgtbl.type_addr=COS_PGTBL_TYPEADDR(start_addr,size_order,num_order,type);
		/* Initialize the page table part to full zero */
		for(count=0;count<10;count++)
			pt->pgtbl.data.pgt[count]=0;
	}
	else {
		pt->pgtbl.type_addr=0;
		/* Initialize this as MPU data */
		for(count=0;count<4;count++) {
			pt->pgtbl.data.mpu[count].rbar=CMX_MPU_VALID|count;
			pt->pgtbl.data.mpu[count].rasr=0;
		}
		/* These two are used to store the MPU occupation status */
		pt->pgtbl.data.pgt[8]=0;
		pt->pgtbl.data.pgt[9]=0;
	}

	pt->refcnt_flags = 1;
	pt->parent = NULL; /* new cap has no parent. only copied cap has. parent is used for referencing */

	__cap_capactivate_post(&pt->h, CAP_PGTBL);

	return 0;
}

int
pgtbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, 
		 livenessid_t lid, capid_t pgtbl_cap, capid_t cosframe_addr, const int root)
{ 
	struct cap_header *deact_header;
	struct cap_pgtbl *deact_cap, *parent;

	unsigned long l, old_v = 0, *pte = NULL;
	int ret;

	/* Find the page table to deactivate */
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
	} else {
		/* more reference exists. */
		if (root) cos_throw(err, -EINVAL);
		assert(!pgtbl_cap && !cosframe_addr);
	}

	if (cos_cas((unsigned long *)&deact_cap->refcnt_flags, l, CAP_MEM_FROZEN_FLAG) != CAS_SUCCESS) cos_throw(err, -ECASFAIL);

	/* deactivation success. We should either release the
	 * page, or decrement parent cnt. */
	if (parent == NULL) { 
		if (ret) {
			cos_faa((int *)&deact_cap->refcnt_flags, 1);
			cos_throw(err, ret);
		}
	} else {
		cos_faa((int*)&parent->refcnt_flags, -1);
	}

	/* FIXME: this should be before the kmem_deact_post */
	ret = cap_capdeactivate(dest_ct_cap, capin, CAP_PGTBL, lid);
	if (ret) cos_throw(err, ret);

	return 0;
err:
	return ret;
}
