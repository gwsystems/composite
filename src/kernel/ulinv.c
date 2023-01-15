#include "cos_types.h"
#include "component.h"
#include "thd.h"
#include "inv.h"

/* walk up the ul-invocation stack to determine the current invoking component */
struct comp_info *
ulinvstk_current(struct ulk_invstk *stk, struct comp_info *origin, unsigned long offset)
{
	struct ulk_invstk_entry   *ent = &stk->stk[0];
	struct comp_info          *ci = origin;
	struct cap_sinv           *sinvcap;
	unsigned long              i;

	/* upper-bounded by ulinvstk sz */
	for (i = offset; i < ULK_INVSTK_NUM_ENT && i < stk->top; i++) {
		ent = &stk->stk[i];
		sinvcap = (struct cap_sinv *)captbl_lkup(ci->captbl, ent->sinv_cap);
		if (!CAP_TYPECHK(sinvcap, CAP_SINV)) return NULL;
		ci = &sinvcap->comp_info;
	}

	return ci;
}
