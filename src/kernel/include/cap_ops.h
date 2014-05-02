#ifndef CAP_OPS
#define CAP_OPS

#include "captbl.h"
#include "pgtbl.h"

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
	h = captbl_add(ct->captbl, capin, type, &ret);
err:
	*retval = ret;
	return h;
}

/* commit the activation */
static inline void
__cap_capactivate_post(struct cap_header *h, cap_t type, u16_t poly)
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
	h->type = type;
	h->poly = poly;
}

static inline int
cap_capdeactivate(struct captbl *t, capid_t cap, capid_t capin, cap_t type)
{ 
	struct cap_captbl *ct;
	
	assert(t);
	ct = (struct cap_captbl *)captbl_lkup(t, cap);
	if (unlikely(!ct)) return -ENOENT;
	if (unlikely(ct->h.type != CAP_CAPTBL)) return -EINVAL;
	return captbl_del(ct->captbl, capin, type); 
}

/* 
 * Page-table-based capability operations for activation and
 * deactivation.
 */

static inline int
cap_memactivate(struct captbl *t, capid_t cap, capid_t capin, u32_t page, u32_t flags)
{
	struct cap_pgtbl *pt;
	
	pt = (struct cap_pgtbl *)captbl_lkup(t, cap);
	if (unlikely(!pt)) return -ENOENT;
	if (unlikely(pt->h.type != CAP_PGTBL)) return -EINVAL;
	return pgtbl_mapping_add(pt->pgtbl, capin, page, flags);
}

static inline int
cap_memdeactivate(struct captbl *t, capid_t cap, unsigned long addr)
{
	struct cap_pgtbl *pt;
	
	assert(t);
	pt = (struct cap_pgtbl *)captbl_lkup(t, cap);
	if (unlikely(!pt)) return -ENOENT;
	if (unlikely(pt->h.type != CAP_PGTBL)) return -EINVAL;
	return pgtbl_mapping_del(pt->pgtbl, addr);
}

static inline int
cap_mem_retype2kern(struct captbl *t, capid_t cap, unsigned long addr, unsigned long *kern_addr)
{
	struct cap_pgtbl *pgtblc;

	assert(t);
	pgtblc = captbl_lkup(t, cap);
	if (unlikely(!pgtblc)) return -ENOENT;
	if (unlikely(pgtblc->h.type != CAP_PGTBL || pgtblc->lvl != 0)) return -EINVAL;
	if ((ret = pgtbl_mapping_extract(pgtblc->pgtbl, addr, (unsigned long *)kern_addr))) return ret;
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
		intern = captbl_lkup_lvl(ct->captbl, expandid, ct->lvl, ctsub->lvl);
		if (!intern)      return -ENOENT;
		if (*intern != 0) return -EPERM;
		/* FIXME: should be cos_cas */
		*intern = (unsigned long)ctsub->captbl; /* commit */
	} else {
		u32_t flags = 0;
		intern = pgtbl_lkup_lvl(((struct cap_pgtbl *)ct)->pgtbl, expandid, &flags, ct->lvl, depth);
		if (!intern)                return -ENOENT;
		if (pgtbl_ispresent(*intern)) return -EPERM;
		/* 
		 * FIXME: need a return value from _set on write
		 * failure; assume ert_intern is essentially a long *.
		 */
		__pgtbl_set((struct ert_intern *)intern, 
			    ((struct cap_pgtbl *)ctsub)->pgtbl, NULL, 0);
	}

	return 0;
}

/* 
 * FIXME: Next version will probably need to provide the capability to
 * the inner node of the captbl/pgtbl so that we can maintain proper
 * reference counting.
 */
static inline int
cap_decons(struct captbl *t, capid_t cap, capid_t pruneid, unsigned long lvl)
{
	struct cap_header *head;
	unsigned long *intern;

	head = (struct cap_header *)captbl_lkup(t, cap);
	if (unlikely(!head)) return -ENOENT;
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
	*intern = 0; /* commit; note that 0 is "no entry" in both pgtbl and captbl */

	return 0;
}

/* 
 * Copy a capability from a location in one captbl/pgtbl to a location
 * in the other.  Fundamental operation used to delegate capabilities.
 * TODO: should limit the types of capabilities this works on.
 */
static inline int
cap_cpy(struct captbl *t, capid_t cap_to, capid_t capin_to, 
	capid_t cap_from, capid_t capin_from, cap_t type)
{
	struct cap_header *ctto, *ctfrom;
	int sz, ret;
	cap_t cap_type;
	
	ctfrom = captbl_lkup(t, cap_from);
	if (unlikely(!ctfrom)) return -ENOENT;
	cap_type = ctfrom->type; 

	if (cap_type == CAP_CAPTBL) {
		ctfrom = captbl_lkup(((struct cap_captbl *)ctfrom)->captbl, capin_from);
		if (unlikely(!ctfrom))              return -ENOENT;
		if (unlikely(ctfrom->type != type)) return -EINVAL;

		sz = __captbl_cap2sz(type);
		ctto = __cap_capactivate_pre(t, cap_to, capin_to, type, &ret);
		if (!ctto) return -EINVAL;
		memcpy(ctto->post, ctfrom->post, sz - sizeof(struct cap_header));
		__cap_capactivate_post(ctto, type, ctfrom->poly);
	} else if (cap_type == CAP_PGTBL) {
		unsigned long *f;
		u32_t flags;

		ctto = captbl_lkup(t, cap_to);
		if (unlikely(!ctto)) return -ENOENT;
		if (unlikely(ctto->type != cap_type)) return -EINVAL;

		f = pgtbl_lkup(((struct cap_pgtbl *)ctfrom)->pgtbl, capin_from, &flags);
		if (!f) return -ENOENT;
		
		/* TODO: validate the type is appropriate given the value of *flags */
		ret = pgtbl_mapping_add(((struct cap_pgtbl *)ctto)->pgtbl, 
					capin_to, *f & PGTBL_FRAME_MASK, flags);
	} else {
		ret = -EINVAL;
	}
	return ret;
}

#endif	/* CAP_OPS */
