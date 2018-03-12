#include <capmgr.h>
#include <schedinit.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include "sched_info.h"

extern int parent_schedinit_child(void);

extern unsigned int self_init;
static int num_child_init = 0;

int
schedinit_child(void)
{
	spdid_t c = cos_inv_token();
	thdcap_t thdcap = 0;
	thdid_t  thdid  = 0;
	struct cos_defcompinfo *dci;
	struct sched_childinfo *ci;

	if (!c) return -1;
	ci  = sched_childinfo_find(c);
	/* is a child sched? */
	if (!ci || !(ci->flags & COMP_FLAG_SCHED)) return -1;
	dci = sched_child_defci_get(ci);
	if (!dci) return -1;

	/* thd retrieve */
	do {
		struct sl_thd *t = NULL;

		thdcap = capmgr_thd_retrieve_next(c, &thdid);
		if (!thdid) break;
		t = sl_thd_lkup(thdid);
		/* already in? only init thd, coz it's created by this sched! */
		if (unlikely(t)) continue;

		t = sl_thd_ext_init(thdcap, 0, 0, 0);
		if (!t) return -1;
	} while (thdid);
	num_child_init++;

	return 0;
}

int
schedinit_self(void)
{
	int unused;

	/* if my init is done and i've all child inits */
	if (self_init && num_child_init == sched_num_child_get())
		return parent_schedinit_child();


	return 0;
}
