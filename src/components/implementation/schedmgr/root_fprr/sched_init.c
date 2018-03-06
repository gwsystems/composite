#include <capmgr.h>
#include <schedinit.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>

extern unsigned int self_init;
extern u64_t childsch_bitf;
static u64_t childinit_bitf = 0;
extern struct cos_defcompinfo *child_defci_get(spdid_t spdid);

int
schedinit_child_intern(spdid_t c)
{
	thdcap_t thdcap = 0;
	thdid_t  thdid  = 0;
	struct cos_defcompinfo *dci;

	/* is a child sched? */
	if (!(childsch_bitf & ((u64_t)1 << (c-1)))) return -1;
	dci = child_defci_get(c);
	if (!dci) return -1;

	/* thd retrieve */
	do {
		struct sl_thd *t = NULL;

		thdid = capmgr_thd_retrieve_next(c, &thdcap);
		if (!thdid) break;
		t = sl_thd_lkup(thdid);
		/* already in? only init thd, coz it's created by this sched! */
		if (unlikely(t)) continue;

		t = sl_thd_ext_init(thdcap, 0, 0, 0);
		if (!t) return -1;
	} while (thdid);

	/* set childinit bitf */
	childinit_bitf &= ((u64_t)1 << (c-1));

	return 0;
}

int
schedinit_self(void)
{
	/* if my init is done and i've all child inits */
	if (self_init && (childinit_bitf == childsch_bitf))
		return 1;

	return 0;
}
