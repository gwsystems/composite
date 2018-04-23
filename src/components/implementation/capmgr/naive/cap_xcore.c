/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cap_info.h>

/* SINV TOKEN == cap_comm_info pointer */
int
cap_xcore_asnd(int arg2, int arg3, int yield)
{
	cycles_t now, win;
	struct cap_comm_info *commci = (struct cap_comm_info *)cos_inv_token();

	if (unlikely(!commci || commci->rcvcpuid == cos_cpuid())) return -EINVAL;
	if (unlikely(!commci->sndcap[cos_cpuid()])) return -EINVAL;
	if (unlikely(!commci->ipimax)) goto done;

	win = commci->ipiwin_start;
	rdtscll(now);
	if ((win + commci->ipiwin) <= now) {
		u32_t cnt = ps_load((unsigned long *)&commci->ipicnt);

		/*
		 * 64bit CAS using GCC built-in.
		 * If cas fails, assume the other thread has reset the ipicnt too.
		 * And if it hasn't yet, then we have some inaccuracies in rate-limiting.
		 */
		if (__sync_bool_compare_and_swap(&commci->ipiwin_start,
						 win, now) == true) ps_cas((unsigned long *)&commci->ipicnt, cnt, 0);
	} else if(commci->ipicnt >= commci->ipimax) {
		return -EDQUOT;
	}

	ps_faa((unsigned long *)&commci->ipicnt, 1);

done:
	return cos_asnd(commci->sndcap[cos_cpuid()], yield);
}
