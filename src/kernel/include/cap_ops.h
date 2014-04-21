#ifndef CAP_OPS
#define CAP_OPS

#include "captbl.h"
#include "pgtbl.h"

static inline struct cap_header *
__cap_capactivate_pre(struct captbl *t, unsigned long cap, unsigned long capin, cap_t type, int *retval)
{
	struct cap_captbl *ct;
	struct cap_header *h = NULL;
	int ret;
	
	ct = (struct cap_captbl *)captbl_lkup(t, cap);
	if (unlikely(!ct || ct->h.type != CAP_CAPTBL)) cos_throw(err, -EINVAL);
	h = captbl_add(ct, capin, type, &ret);
err:
	*retval = ret;
	return h;
}

/* commit the activation */
static inline int
__cap_capactivate_post(struct cap_header *h, cap_t type, u16_t poly)
{
	/* 
	 * FIXME: should be atomic on a word including the amap and
	 * poly.  Should also be a store barrier here
	 */
	h->type = type;
}

static inline int
cap_capdeactivate(struct captbl *t, unsigned long cap, unsigned long capin, cap_t type)
{ 
	struct cap_captbl *ct;
	int ret;
	
	assert(t);
	ct = captbl_lkup(t, cap);
	if (unlikely(!ct)) return -ENOENT;
	if (unlikely(ct->h.type != CAP_CAPTBL)) return -EINVAL;
	return captbl_del(ct, capin, type); 
}

static inline int
cap_memactivate(struct captbl *t, unsigned long cap, unsigned long addr, u32_t page, u32_t flags)
{
	struct cap_pgtbl *pt;
	
	pt = (struct cap_pgtbl *)captbl_lkup(t, cap);
	if (unlikely(!pt || pt->h.type != CAP_PGTBL)) return -EINVAL;
	return pgtbl_mapping_add(pt->pgtbl, capin, page, flags);
}

static inline int
cap_memdeactivate(struct captbl *t, unsigned long cap, unsigned long addr)
{
	struct cap_pgtbl *pt;
	int ret;
	
	assert(t);
	pt = captbl_lkup(t, cap);
	if (unlikely(!pt)) return -ENOENT;
	if (unlikely(pt->h.type != CAP_PGTBL)) return -EINVAL;
	return pgtbl_mapping_del(pt->pgtbl, addr);
}

static inline int
cap_cons(struct captbl *t, unsigned long capto, unsigned long capsub, unsigned long expandid)
{
	struct cap_header *ctto, *ctsub;
	struct cap_captbl *__ctsub;
	int ret;
	u32_t depth;
	cap_t captbl_type;

	if (unlikely(capto == capsub)) return -EINVAL;
	ctto = captbl_lkup(t, cap_to);
	if (unlikely(!ctto))           return -ENOENT;
	captbl_type = ctto->type; 
	if (unlikely(captbl_type != CAP_CAPTBL && 
		     captbl_type != CAP_PGTBL)) return -EINVAL;
	ctsub = captbl_lkup(t, capsub);
	if (unlikely(!ctsub)) return -ENOENT;
	if (unlikely(ctsub->type != captbl_type)) return -EINVAL;
	depth = ctsub->lvl;
	if (depth == 0) return -EINVAL; /* subtree must not have a root */

	
}

/* 
 * Copy a capability from a location in one captbl/pgtbl to a location
 * in the other.  TODO: should limit the types of capabilities this
 * works on.
 */
static inline int
cap_cpy(struct captbl *t, unsigned long cap_to, unsigned long capin_to, 
	unsigned long cap_from, unsigned long capin_from, cap_t type)
{
	struct cap_header *ctto, *ctfrom;
	int ret, sz;
	cap_t captbl_type;
	
	ctto = captbl_lkup(t, cap_to);
	if (unlikely(!ctto)) return -ENOENT;
	captbl_type = ctto->type; 
	if (unlikely(captbl_type != CAP_CAPTBL && 
		     captbl_type != CAP_PGTBL)) return -EINVAL;
	if (cap_to != cap_from) {
		ctfrom = captbl_lkup(t, cap_from);
		if (unlikely(!ctfrom)) return -ENOENT;
		if (unlikely(ctfrom->type != captbl_type)) return -EINVAL;
	} else {
		ctfrom = ctto;
	}

	ctfrom = captbl_lkup((struct cap_captbl *)ctfrom, capin_from);
	if (unlikely(!ctfrom))                return -ENOENT;
	if (unlikely(ctfrom->type != type)) return -EINVAL;
	ctto = captbl_lkup((struct cap_captbl *)ctto, capin_to);
	if (unlikely(!ctto))                  return -ENOENT;
	if (unlikely(ctto->type != type))   return -EINVAL;

	sz = __captbl_cap2sz(type);
	memcpy(ctto->post, ctfrom->post, sz - sizeof(struct cap_header));
}

#endif	/* CAP_OPS */
