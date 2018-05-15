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

extern thdcap_t capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid);
extern void rk_dummy_thdinit(thdid_t, int);

cbuf_t
schedinit_child(void)
{
	spdid_t c = cos_inv_token();
	struct cos_defcompinfo *dci;
	struct sl_thd *tcur;

	if (!c) return 0;
	tcur = sl_thd_curr();
	if (!tcur) return 0;

	/* this component is only for applications..*/
	rk_dummy_thdinit(sl_thd_thdid(tcur), 0);

	return 0;
}
