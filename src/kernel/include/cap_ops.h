#ifndef CAP_OPS
#define CAP_OPS

#include "captbl.h"
#include "pgtbl.h"
#include "liveness_tbl.h"

/*
 * Capability-table, capability operations for activation and
 * deactivation.
 */

static inline struct cap_header *
__cap_capactivate_pre(struct captbl *t, capid_t cap, capid_t capin, cap_t type, int *retval)
{
	struct cap_captbl *ct;
	struct cap_header *h = NULL;
	int ret;

	ct = (struct cap_captbl *)captbl_lkup(t, cap);
	/* Add level check here */
	if (unlikely(!ct || ct->h.type != CAP_CAPTBL)) cos_throw(err, -EINVAL);
	if (unlikely(ct->refcnt_flags & CAP_MEM_FROZEN_FLAG)) cos_throw(err, -EINVAL);

	h = captbl_add(ct->captbl, capin, type, &ret);
err:
	*retval = ret;
	return h;
}

/* commit the activation */
static inline int
__cap_capactivate_post(struct cap_header *h, cap_t type)
{
	/*
	 * FIXME: should be atomic on a word including the amap and
	 * poly, mostly likely atomic add on the type/poly (they are
	 * contiguous) to avoid any conflicts at this point on amap
	 * modifications.
	 *
	 * Should also be a store barrier here as well to make sure
	 * the modifications to the capability body are committed
	 * before finally activating the cap.
	 */

	/* u32_t old_v, new_v; */
	/* struct cap_header *local; */

	cos_mem_fence();

	/* FIXME: the following is done in captbl_add now, which is
	 * wrong. */
/*
	new_v = old_v = *((u32_t *)h);

	local = (struct cap_header *)&new_v;
	local->type = type;

	if (unlikely(!cos_cas((unsigned long *)h, old_v, new_v))) return -ECASFAIL;
*/

	return 0;
}

static inline int
cap_capdeactivate(struct cap_captbl *ct, capid_t capin, cap_t type, livenessid_t lid)
{
	if (unlikely(!ct)) return -ENOENT;
	if (unlikely(ct->h.type != CAP_CAPTBL)) return -EINVAL;

	return captbl_del(ct->captbl, capin, type, lid);
}

static inline int
cap_kmem_activate(struct captbl *t, capid_t cap, unsigned long addr, unsigned long *kern_addr, unsigned long **pte_ret)
{
	int ret;
	struct cap_pgtbl *pgtblc;

	assert(t);
	pgtblc = (struct cap_pgtbl *)captbl_lkup(t, cap);

	if (unlikely(!pgtblc)) return -ENOENT;

	if (unlikely(pgtblc->h.type != CAP_PGTBL )) return -EINVAL;
	if ((ret = pgtbl_kmem_act(&(pgtblc->pgtbl), addr & PGTBL_FRAME_MASK, (unsigned long *)kern_addr, pte_ret))) return ret;

	return 0;
}

/*
 * Construction and deconstruction of the capability and page tables
 * from separate capability/page-table trees that are subtrees for
 * different levels.
 */
/*
 *
 */
/* PRY: this is dedicated to captbls */
static inline int
cap_cons(struct captbl *t, capid_t capto, capid_t capsub, capid_t expandid)
{
	/*
	 * Note: here we're relying on the fact that cap_captbl has an
	 * identical layout to cap_pgtbl.
	 */
	struct cap_captbl *ct, *ctsub;
	u32_t *intern;
	u32_t depth;
	cap_t cap_type;
	int ret = 0;

	if (unlikely(capto == capsub)) return -EINVAL;
	ct = (struct cap_captbl *)captbl_lkup(t, capto);
	if (unlikely(!ct))             return -ENOENT;
	cap_type = ct->h.type;
	if (unlikely(cap_type != CAP_CAPTBL && cap_type != CAP_PGTBL)) return -EINVAL;
	ctsub = (struct cap_captbl *)captbl_lkup(t, capsub);
	if (unlikely(!ctsub))                    return -ENOENT;
	if (unlikely(ctsub->h.type != cap_type)) return -EINVAL;

	depth = ctsub->lvl;
	if (depth == 0) return -EINVAL; /* subtree must not have a root */

	assert(ct->captbl);
	if (cap_type == CAP_CAPTBL) {
		ret = captbl_cons(ct, ctsub, expandid);
	} else {
		ret = 0;
	}

	return ret;
}

/* PRY:this is dedicated to pgtbls */
extern u32_t chal_mpu_update(struct pgtbl* table, u32_t op_flag);
static inline int
pgtbl_cons(struct captbl *t, capid_t capto, capid_t capsub, capid_t expandid)
{
	/*
	 * Note: here we're relying on the fact that cap_captbl has an
	 * identical layout to cap_pgtbl.
	 */
	struct cap_captbl *ct, *ctsub;
	u32_t *intern;
	u32_t depth;
	cap_t cap_type;
	int ret = 0;

	if (unlikely(capto == capsub)) return -EINVAL;
	ct = (struct cap_captbl *)captbl_lkup(t, capto);
	if (unlikely(!ct))             return -ENOENT;
	cap_type = ct->h.type;
	if (unlikely(cap_type != CAP_PGTBL)) return -EINVAL;
	ctsub = (struct cap_captbl *)captbl_lkup(t, capsub);
	if (unlikely(!ctsub))                    return -ENOENT;
	if (unlikely(ctsub->h.type != cap_type)) return -EINVAL;

	/* PRY: no need to care about cons/decons */
	/* FIXME: we need to ensure TLB quiescence for pgtbl cons/decons! */
	u32_t flags = 0, old_pte, new_pte, old_v, refcnt_flags;
	u32_t old_flags;
	u32_t size_order;
	struct cap_pgtbl *cpsub=(struct cap_pgtbl *)ctsub;
	struct cap_pgtbl *cp=(struct cap_pgtbl *)ct;

	/* look up the position to add this to. If this is the top-level, we ignore that expandid and start direct insertion */
	if(((cp->pgtbl.type_addr)&COS_PGTBL_INTERN)==0) {
		/* Do we have a lower level? */
		intern = (u32_t*)&(cp->pgtbl.type_addr);
		/* For top-level MPU metadata nodes, this field is organized as
		 * [31 Ptr 2][1 Intern 0]. Must be 0 to make sure that
		 * the lower level are not constructed into it */
		if((cp->pgtbl.type_addr)!=0)
			return -EPERM;
		old_pte = *intern;
	}
	else {
		/* Only do this if we are not cons'ing into the higher levels */
		intern = pgtbl_lkup_leaf(&(cp->pgtbl),expandid,&size_order,&old_flags);
		if (!intern)                  return -ENOENT;
		old_pte = *intern;
		if (pgtbl_ispresent(old_pte)) return -EPERM;
	}

	old_v = refcnt_flags = cpsub->refcnt_flags;
	if (refcnt_flags & CAP_MEM_FROZEN_FLAG) return -EINVAL;
	if ((refcnt_flags & CAP_REFCNT_MAX) == CAP_REFCNT_MAX) return -EOVERFLOW;

	refcnt_flags++;
	ret = cos_cas((unsigned long *)&(cpsub->refcnt_flags), old_v, refcnt_flags);
	if (ret != CAS_SUCCESS) return -ECASFAIL;

	/* TODO:Check if it is allowed to put the pte here, start address checking */
	/* Does the child already have a ultimate top-level, or itself is a designated top-level? if yes, we cannot cons. */

	if(((cpsub->pgtbl.type_addr)&COS_PGTBL_INTERN)==0)
		return -EPERM;
	else if(cpsub->pgtbl.data.pgt[9]!=0)
		return -EPERM;

	/* PRY : See if the parent table is a designated the top-level */
	if(((cp->pgtbl.type_addr)&COS_PGTBL_INTERN)==0) {
		/* Yes, this is the top-level. don't perform checks, always allow CONS */
	}
	else {
		/* See if the this table have a ultimate top-level. If this field is zero then
		 * it means that it does not have a ultimate top-level, we allow the cons, or
		 * we do not allow the cons */
		if((cp->pgtbl.data.pgt[9])==0)
			 return -EPERM;

		/* Check if the parameters passed in is allowed, within range */
		if((COS_PGTBL_NODE_ADDR(cp->pgtbl.type_addr)+(1<<COS_PGTBL_SIZEORDER(cp->pgtbl.type_addr))*expandid)>
		   COS_PGTBL_NODE_ADDR(cpsub->pgtbl.type_addr))
			return -EPERM;

		if((COS_PGTBL_NODE_ADDR(cp->pgtbl.type_addr)+(1<<COS_PGTBL_SIZEORDER(cp->pgtbl.type_addr))*(expandid+1))<=
		   COS_PGTBL_NODE_ADDR(cpsub->pgtbl.type_addr))
			return -EPERM;
	}

	/* PRY:Check complete, can do the mapping, and register the parent */
	if(((cp->pgtbl.type_addr)&COS_PGTBL_INTERN)==0)
		new_pte = COS_PGTBL_CHILD(&(cpsub->pgtbl),0);
	else
		new_pte = COS_PGTBL_CHILD(&(cpsub->pgtbl),PGTBL_INTERN_DEF);

	cpsub->pgtbl.data.pgt[9]=(u32_t)&(cp->pgtbl);

	ret = cos_cas((unsigned long *)intern, old_pte, new_pte);
	if (ret != CAS_SUCCESS) {
		/* decrement to restore the refcnt on failure. */
		cos_faa((int *)&(cpsub->refcnt_flags), -1);
		return -ECASFAIL;
	} else {
		ret = 0;
	}

	/* PRY: update the MPU metadata eagerly */
	chal_mpu_update(&(cpsub->pgtbl),1);

	return ret;
}

/*
 * FIXME: Next version will probably need to provide the capability to
 * the inner node of the captbl/pgtbl so that we can maintain proper
 * reference counting.
 */
/* PRY:This function may not work for CM7, may need more work */
static inline int
cap_decons(struct captbl *t, capid_t cap, capid_t capsub, capid_t pruneid, unsigned long lvl)
{
	/* capsub is the cap_cap for sub level to be pruned. We need
	 * to decrement ref_cnt correctly for the kernel page. */
	struct cap_header *head, *sub;
	u32_t *intern, old_v;

	head = (struct cap_header *)captbl_lkup(t, cap);
	sub  = (struct cap_header *)captbl_lkup(t, capsub);
	if (unlikely(!head || !sub)) return -ENOENT;
	if (unlikely(head->type != sub->type)) return -EPERM;

	if (head->type == CAP_CAPTBL) {
		struct cap_captbl *ct = (struct cap_captbl *)head;

		if (lvl <= ct->lvl) return -EINVAL;
		intern = captbl_lkup_lvl(ct->captbl, pruneid, ct->lvl, lvl);
	} else if (head->type == CAP_PGTBL) {
		struct cap_pgtbl *pt = (struct cap_pgtbl *)head;
		u32_t size_order;
		u32_t old_flags;
		intern = (u32_t*)pgtbl_lkup_leaf(&(pt->pgtbl), pruneid, &size_order, &old_flags);
	} else {
		return -EINVAL;
	}

	if (!intern) return -ENOENT;
	old_v = *intern;

	if (old_v == 0) return 0; /* return an error here? */
	/* commit; note that 0 is "no entry" in both pgtbl and captbl */
	if (cos_cas((unsigned long *)intern, old_v, 0) != CAS_SUCCESS) return -ECASFAIL;

	if (head->type == CAP_CAPTBL) {
		/* FIXME: we are removing two half pages for captbl. */
		struct cap_captbl *ct = (struct cap_captbl *)head;

		intern = captbl_lkup_lvl(ct->captbl, pruneid+(PAGE_SIZE/2/CAPTBL_LEAFSZ), ct->lvl, lvl);
		if (!intern) return -ENOENT;

		old_v = *intern;
		if (old_v == 0) return 0; /* return an error here? */
		/* commit; note that 0 is "no entry" in both pgtbl and captbl */
		if (cos_cas((unsigned long *)intern, old_v, 0) != CAS_SUCCESS) return -ECASFAIL;
	}

	/* decrement the refcnt */
	if (head->type == CAP_CAPTBL) {
		struct cap_captbl *ct = (struct cap_captbl *)sub;
		u32_t old_v, l;

		old_v = l = ct->refcnt_flags;
		if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
		cos_faa((int*)&(ct->refcnt_flags), -1);
	} else {
		struct cap_pgtbl *pt = (struct cap_pgtbl *)sub;
		u32_t old_v, l;

		old_v = l = pt->refcnt_flags;
		if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
		cos_faa((int*)&(pt->refcnt_flags), -1);
	}

	return 0;
}

static int
cap_kmem_freeze(struct captbl *t, capid_t target_cap)
{
	struct cap_header *ch;
	u32_t l;
	int ret;

	ch = captbl_lkup(t, target_cap);
	if (!ch) return -EINVAL;

	/* Only memory for captbl and pgtbl needs to be frozen before
	 * deactivation. */
	if (ch->type == CAP_CAPTBL) {
		struct cap_captbl *ct = (struct cap_captbl *)ch;
 		l = ct->refcnt_flags;

		if ((l & CAP_REFCNT_MAX) > 1 || l & CAP_MEM_FROZEN_FLAG) return -EINVAL;

		rdtscll(ct->frozen_ts);
		ret = cos_cas((unsigned long *)&ct->refcnt_flags, l, l | CAP_MEM_FROZEN_FLAG);
		if (ret != CAS_SUCCESS) return -ECASFAIL;
	} else if (ch->type == CAP_PGTBL) {
		struct cap_pgtbl *pt = (struct cap_pgtbl *)ch;
		l = pt->refcnt_flags;
		if ((l & CAP_REFCNT_MAX) > 1 || l & CAP_MEM_FROZEN_FLAG) return -EINVAL;

		rdtscll(pt->frozen_ts);
		ret = cos_cas((unsigned long *)&pt->refcnt_flags, l, l | CAP_MEM_FROZEN_FLAG);
		if (ret != CAS_SUCCESS) return -ECASFAIL;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
kmem_page_scan(void *obj_vaddr, const int size)
{
	/* For non-leaf level captbl / pgtbl. entries are all pointers
	 * in these cases. */
	unsigned int i;
	void *addr = obj_vaddr;

	for (i = 0; i < size / sizeof(void *); i++) {
		if (*(unsigned long *)addr != 0) return -EINVAL;
		addr++;
	}

	return 0;
}

int kmem_deact_pre(struct cap_header *ch, struct captbl *ct, capid_t pgtbl_cap,
	       capid_t cosframe_addr, unsigned long **p_pte, unsigned long *v);
int kmem_deact_post(unsigned long *pte, unsigned long old_v);

#endif	/* CAP_OPS */
