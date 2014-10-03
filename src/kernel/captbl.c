#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/cap_ops.h"

/* 
 * Add the capability table to itself at cap.  This should really only
 * be used at boot time to create the initial bootable components.
 * This and page-table creation are the only special cases for booting
 * the system.
 */
int 
captbl_activate_boot(struct captbl *t, unsigned long cap)
{
	struct cap_captbl *ctc;
	int ret;

	ctc = (struct cap_captbl *)captbl_add(t, cap, CAP_CAPTBL, &ret);
	if (!ctc) return ret;
	ctc->captbl = t; 	/* reference ourself! */
	ctc->lvl    = 0;
	ctc->h.type = CAP_CAPTBL;

	return 0;
}

int
captbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, struct captbl *toadd, u32_t lvl)
{
	struct cap_captbl *ct;
	int ret;
	
	ct = (struct cap_captbl *)__cap_capactivate_pre(t, cap, capin, CAP_CAPTBL, &ret);
	if (!unlikely(ct)) return -1;

	ct->captbl = toadd;
	ct->lvl    = lvl;
	ct->refcnt = 1;
	ct->parent = NULL; /* new cap has no parent. only copied cap has. */

	__cap_capactivate_post(&ct->h, CAP_CAPTBL);

	return 0;
}

int captbl_deactivate(struct captbl *t, struct cap_captbl *dest_ct_cap, unsigned long capin, livenessid_t lid,
		      livenessid_t kmem_lid, capid_t pgtbl_cap, capid_t cosframe_addr)
{
	struct cap_header *deact_header;
	struct cap_captbl *deact_cap, *parent;

	unsigned long old_v = 0, *pte = NULL;
	int ret;

	deact_header = captbl_lkup(dest_ct_cap->captbl, capin);
	if (!deact_header || deact_header->type != CAP_CAPTBL) cos_throw(err, -EINVAL);

	deact_cap = (struct cap_captbl *)deact_header;
	assert(deact_cap->refcnt);
	parent   = deact_cap->parent;

	if (deact_cap->refcnt != 1) {
		/* We need to deact children first! */
		cos_throw(err, -EINVAL);
	}

	if (parent == NULL) {
		/* Last reference to the captbl page. Require pgtbl
		 * and cos_frame cap to release the kmem page. */
		ret = kmem_deact_pre(t, pgtbl_cap, cosframe_addr, kmem_lid, 
				     (void *)(deact_cap->captbl), &pte, &old_v);
		if (ret) cos_throw(err, ret);
	} else {
		/* more reference exists. just sanity
		 * checks. */
		if (pgtbl_cap || cosframe_addr || kmem_lid) {
			/* we pass in the pgtbl cap and frame addr,
			 * but ref_cnt is > 1. We'll ignore the two
			 * parameters as we won't be able to release
			 * the memory. */
			printk("cos: deactivating captbl but not able to release kmem page (%p) yet (ref_cnt %d).\n", 
			       (void *)cosframe_addr, deact_cap->refcnt);
		}
	}

	ret = cap_capdeactivate(dest_ct_cap, capin, CAP_CAPTBL, lid); 
	
	if (ret == 0) {
		ret = cos_cas((unsigned long *)&deact_cap->refcnt, 1, 0);
		if (ret != CAS_SUCCESS) {
			cos_throw(err, -ECASFAIL);
		}

		/* deactivation success. We should either release the
		 * page, or decrement parent cnt. */
		if (parent == NULL) { 
			/* move the kmem to COSFRAME */
			kmem_deact_post(pte, old_v, kmem_lid);
		} else {
			cos_faa(&parent->refcnt, -1);
		}
	} else {
		cos_throw(err, ret);
	}

	return 0;
err:
	return ret;
}

int
captbl_cons(struct captbl *ct, capid_t target, capid_t cons_addr, capid_t cons_capid)
{
	int ret;
	void *captbl_mem;
	struct cap_captbl *target_ct, *cons_cap;
			
	target_ct = (struct cap_captbl *)captbl_lkup(ct, target);
	if (target_ct->h.type != CAP_CAPTBL || target_ct->lvl != 0) cos_throw(err, -EINVAL);

	cons_cap = (struct cap_captbl *)captbl_lkup(ct, cons_capid);
	if (cons_cap->h.type != CAP_CAPTBL || cons_cap->lvl != 1) cos_throw(err, -EINVAL);

	captbl_mem = (void *)cons_cap->captbl;

	cos_faa(&cons_cap->refcnt, 1);
	/* FIXME: we are expanding the entire page to
	 * two of the locations. Do we want separate
	 * calls for them? */
	captbl_init(captbl_mem, 1);
	ret = captbl_expand(target_ct->captbl, cons_addr, captbl_maxdepth(), captbl_mem);
	if (ret) {
		cos_faa(&cons_cap->refcnt, -1);
		cos_throw(err, ret);
	}

	captbl_init(&((char*)captbl_mem)[PAGE_SIZE/2], 1);
	ret = captbl_expand(target_ct->captbl, cons_addr + (PAGE_SIZE/2/CAPTBL_LEAFSZ), 
			    captbl_maxdepth(), &((char*)captbl_mem)[PAGE_SIZE/2]);
	if (ret) {
		/* Rewind. */
		captbl_expand(target_ct->captbl, cons_addr, captbl_maxdepth(), NULL);
		cos_faa(&cons_cap->refcnt, -1);
		cos_throw(err, ret);
	}
	
	return 0;
err:
	return ret;
}
