#include <captbl.h>
#include <cap_ops.h>

int
captbl_activate_captbl(struct captbl *t, unsigned long cap, unsigned long capin, struct captbl *toadd, u32_t lvl)
{
	struct cap_captbl *ct;
	int ret;
	
	ct = __cap_activate_pre(t, cap, capin, CAP_CAPTBL, &ret);
	if (!unlikely(ct)) return ret;
	ct->captbl = toadd;
	ct->captbl = lvl;
	ct = __cap_activate_post(&ct->h, CAP_CAPTBL, 0);

	return 0;
}

int captbl_deactivate_captbl(struct captbl *t, unsigned long cap, unsigned long capin)
{ return cap_deactivate(t, cap, capin, CAP_CAPTBL); }

void cap_init(void) { return; }
