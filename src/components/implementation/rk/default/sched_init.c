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

cbuf_t
schedinit_child(void)
{
	spdid_t c = cos_inv_token();
	struct cos_defcompinfo *dci;
	struct sl_thd *tcur;

	if (!c) return 0;
	tcur = sl_thd_curr();
	if (!tcur) return 0;

	return 0;
}
