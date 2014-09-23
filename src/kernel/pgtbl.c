#include "include/shared/cos_types.h"
#include "include/captbl.h"
#include "include/pgtbl.h"
#include "include/cap_ops.h"

int
pgtbl_activate(struct captbl *t, unsigned long cap, unsigned long capin, pgtbl_t pgtbl, u32_t lvl)
{
	struct cap_pgtbl *pt;
	int ret;
	
	pt = (struct cap_pgtbl *)__cap_capactivate_pre(t, cap, capin, CAP_PGTBL, &ret);
	if (!unlikely(pt)) return ret;
	pt->pgtbl = pgtbl;
	pt->lvl = lvl;
	__cap_capactivate_post(&pt->h, CAP_PGTBL);

	return 0;
}

int pgtbl_deactivate(struct cap_captbl *t, unsigned long capin, livenessid_t lid)
{ return cap_capdeactivate(t, capin, CAP_PGTBL, lid); }
