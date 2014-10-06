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
	u32_t old_v, new_v;
	struct cap_header *local;

	cos_mem_fence();
	new_v = old_v = *((u32_t *)h);
	
	local = (struct cap_header *)&new_v;
	local->type = type;
	
	if (unlikely(!cos_cas((unsigned long *)h, old_v, new_v))) return -ECASFAIL;

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
cap_kmem_activate(struct captbl *t, capid_t cap, unsigned long addr, unsigned long *kern_addr)
{
	int ret;
	struct cap_pgtbl *pgtblc;

	assert(t);
	pgtblc = (struct cap_pgtbl *)captbl_lkup(t, cap);

	if (unlikely(!pgtblc)) return -ENOENT;
	if (unlikely(pgtblc->h.type != CAP_PGTBL || pgtblc->lvl != 0)) return -EINVAL;
	if ((ret = pgtbl_kmem_act(pgtblc->pgtbl, addr, (unsigned long *)kern_addr))) return ret;

	return 0;
}

/*  
 * Construction and deconstruction of the capability and page tables
 * from separate capability/page-table trees that are subtrees for
 * different levels.
 */

static inline int
cap_cons(struct captbl *t, capid_t capto, capid_t capsub, capid_t expandid)
{
	/* 
	 * Note: here we're relying on the fact that cap_captbl has an
	 * identical layout to cap_pgtbl.
	 */
	struct cap_captbl *ct, *ctsub;
	unsigned long *intern;
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
		u32_t flags = 0, old_v, refcnt_flags;

		intern = pgtbl_lkup_lvl(((struct cap_pgtbl *)ct)->pgtbl, expandid, &flags, ct->lvl, depth);
		if (!intern)                return -ENOENT;
		if (pgtbl_ispresent(*intern)) return -EPERM;

		old_v = refcnt_flags = ((struct cap_pgtbl *)ctsub)->refcnt_flags;
		if (refcnt_flags & CAP_MEM_FROZEN_FLAG) return -EINVAL;
		if ((refcnt_flags & CAP_REFCNT_MAX) == CAP_REFCNT_MAX) return -EOVERFLOW;

		refcnt_flags++;
		ret = cos_cas((unsigned long *)&(((struct cap_pgtbl *)ctsub)->refcnt_flags), old_v, refcnt_flags);
		if (ret != CAS_SUCCESS) return -ECASFAIL;

		ret = __pgtbl_set((struct ert_intern *)intern, 
				  ((struct cap_pgtbl *)ctsub)->pgtbl, NULL, 0);
		if (ret) {
			/* decrement to restore the refcnt on failure. */
			cos_faa((int *)&(((struct cap_pgtbl *)ctsub)->refcnt_flags), -1);
		}
	}

	return ret;
}

/* 
 * FIXME: Next version will probably need to provide the capability to
 * the inner node of the captbl/pgtbl so that we can maintain proper
 * reference counting.
 */
static inline int
cap_decons(struct captbl *t, capid_t cap, capid_t capsub, capid_t pruneid, unsigned long lvl)
{
	/* capsub is the cap_cap for sub level to be pruned. We need
	 * to decrement ref_cnt correctly for the kernel page. */
	struct cap_header *head, *sub;
	unsigned long *intern;

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
		u32_t flags;
		if (lvl <= pt->lvl) return -EINVAL;
		intern = pgtbl_lkup_lvl(pt->pgtbl, pruneid, &flags, pt->lvl, lvl);
	} else {
		return -EINVAL;
	}

	if (!intern) return -ENOENT;
	if (*intern == 0) return 0; /* return an error here? */
	/* FIXME: should be cos_cas */
	/* commit; note that 0 is "no entry" in both pgtbl and captbl */
	if (cos_cas(intern, *intern, 0) != CAS_SUCCESS) return -ECASFAIL;

	/* decrement the refcnt */
	if (head->type == CAP_CAPTBL) {
		struct cap_captbl *ct = (struct cap_captbl *)sub;
		u32_t old_v, l;

		old_v = l = ct->refcnt_flags;
		if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
		cos_cas((unsigned long *)&(ct->refcnt_flags), old_v, l - 1);
	} else {
		struct cap_pgtbl *pt = (struct cap_pgtbl *)sub;
		u32_t old_v, l;

		old_v = l = pt->refcnt_flags;
		if (l & CAP_MEM_FROZEN_FLAG) return -EINVAL;
		cos_cas((unsigned long *)&(pt->refcnt_flags), old_v, l - 1);
	}

	return 0;
}

#endif	/* CAP_OPS */
