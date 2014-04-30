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
	/* FIXME: atomic op required */
	ctc->h.type = CAP_CAPTBL;
	return 0;
}

int
captbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, struct captbl *toadd, u32_t lvl)
{
	struct cap_captbl *ct;
	int ret;
	
	ct = (struct cap_captbl *)__cap_capactivate_pre(t, cap, capin, CAP_CAPTBL, &ret);
	if (!unlikely(ct)) return ret;
	ct->captbl = toadd;
	ct->lvl = lvl;
	__cap_capactivate_post(&ct->h, CAP_CAPTBL, 0);

	return 0;
}

int captbl_deactivate(struct captbl *t, unsigned long cap, unsigned long capin)
{ return cap_capdeactivate(t, cap, capin, CAP_CAPTBL); }
