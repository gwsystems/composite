/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cap_info.h>

/* SINV TOKEN == cap_comm_info pointer */
static unsigned long ipi_snd_counters[NUM_CPU] CACHE_ALIGNED;
static unsigned long ipi_rcv_counters[NUM_CPU] CACHE_ALIGNED;

int
cap_xcore_asnd(int arg1, int arg2, int arg3, int yield)
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
	ps_faa(&ipi_snd_counters[cos_cpuid()], 1);
	ps_faa(&ipi_rcv_counters[commci->rcvcpuid], 1);

	return cos_asnd(commci->sndcap[cos_cpuid()], yield);
}

int
cap_xcore_ipi_ctrs_get(cpuid_t core, unsigned int type, unsigned int *sndctr, unsigned int *rcvctr)
{
	int ret = 0;

	if (type > 1) return -EINVAL;
	if (core >= NUM_CPU) return -EINVAL;

	/* capmgr counters return */
	if (type == 1) {
		*sndctr = ps_load(&ipi_snd_counters[core]);
		*rcvctr = ps_load(&ipi_rcv_counters[core]);
	} else {
		unsigned int k_snd_op = 0, k_rcv_op = 0;

		switch(core) {
			case 0: k_snd_op = HW_CORE0_IPI_SND_GET; k_rcv_op = HW_CORE0_IPI_RCV_GET; break;
			case 1: k_snd_op = HW_CORE1_IPI_SND_GET; k_rcv_op = HW_CORE1_IPI_RCV_GET; break;
			case 2: k_snd_op = HW_CORE2_IPI_SND_GET; k_rcv_op = HW_CORE2_IPI_RCV_GET; break;
			case 3: k_snd_op = HW_CORE3_IPI_SND_GET; k_rcv_op = HW_CORE3_IPI_RCV_GET; break;
			case 4: k_snd_op = HW_CORE4_IPI_SND_GET; k_rcv_op = HW_CORE4_IPI_RCV_GET; break;
			case 5: k_snd_op = HW_CORE5_IPI_SND_GET; k_rcv_op = HW_CORE5_IPI_RCV_GET; break;
			default: return -EINVAL;
		}
		ret = cos_introspect(cos_compinfo_get(cos_defcompinfo_curr_get()), BOOT_CAPTBL_SELF_INITHW_BASE, k_snd_op);
		if (ret < 0) return ret;
		*sndctr = ret;
		ret = cos_introspect(cos_compinfo_get(cos_defcompinfo_curr_get()), BOOT_CAPTBL_SELF_INITHW_BASE, k_rcv_op);
		if (ret < 0) return ret;
		*rcvctr = ret;
	}

	return ret;
}
