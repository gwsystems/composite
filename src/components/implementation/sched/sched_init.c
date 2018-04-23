/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <schedinit.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sched_info.h>
#include <sl_child.h>

extern unsigned int num_child_init[];
extern thdcap_t capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid);

cbuf_t
schedinit_child(void)
{
	spdid_t c = cos_inv_token();
	thdid_t thdid  = 0;
	struct cos_defcompinfo *dci;
	struct sched_childinfo *ci;
	struct sl_thd *init;
	cbuf_t id = 0;
	struct sl_thd *tcur;

	if (!c) return 0;
	ci  = sched_childinfo_find(c);
	/* is a child sched? */
	if (!ci || !(ci->flags & COMP_FLAG_SCHED)) return 0;
	dci = sched_child_defci_get(ci);
	if (!dci) return 0;

	init = sched_child_initthd_get(ci);
	if (!init) return 0;
	tcur = sl_thd_curr();
	if (!tcur) return 0;
	assert(tcur->schedthd == init);

	/* thd retrieve */
	do {
		struct sl_thd *t = NULL;
		struct cos_aep_info aep;

		memset(&aep, 0, sizeof(struct cos_aep_info));
		aep.thd = capmgr_thd_retrieve_next(c, &thdid);
		if (!thdid) break;
		t = sl_thd_try_lkup(thdid);
		/* already in? only init thd, coz it's created by this sched! */
		if (unlikely(t)) continue;

		aep.tid = thdid;
		aep.tc  = sl_thd_tcap(sl__globals_cpu()->sched_thd);
		t = sl_thd_init_ext(&aep, init);
		if (!t) return 0;
	} while (thdid);

	/* create a shared-mem region, initialize it with ck_ring and pass it to the child! */
	id = sl_parent_notif_alloc(init);

	num_child_init[cos_cpuid()]++;

	return id;
}
