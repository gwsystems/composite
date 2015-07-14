/**
 * Copyright 2012 by Qi Wang, interwq@gwu.edu; Gabriel Parmer,
 * gparmer@gwu.edu

 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CPUID_H
#define CPUID_H

#include "../../../kernel/include/shared/consts.h"
#include "../../../kernel/include/asm_ipc_defs.h"
#include "../chal_asm_inc.h"

struct cos_cpu_local_info {
	/* orig_sysenter_esp SHOULD be the first variable here. The
	 * sysenter interposition path gets tss from it. */
	unsigned long *orig_sysenter_esp; /* Points to the end of the tss struct in Linux. */
	/***********************************************/
	/* info saved in kernel stack for fast access. */
	unsigned long cpuid;
	void *curr_thd;
	/* cache the stk_top index to save a cacheline access on
	 * inv/ret. Could use a struct here if need to cache multiple
	 * things. (e.g. captbl, etc) */
	int invstk_top;
	unsigned long epoch;
	/***********************************************/
	/* Since this struct resides at the lowest address of the
	 * kernel stack page (along with thread_info struct of
	 * Linux), we store 0xDEADBEEF to detect overflow. */
	unsigned long overflow_check;
};

static inline struct cos_cpu_local_info *
cos_cpu_local_info(void)
{
	int local;

	return (struct cos_cpu_local_info *)(round_up_to_page((unsigned long)&local) - STK_INFO_OFF);
}

static inline int
get_cpuid(void)
{
#if NUM_CPU > 1
#error "Baremetal does not support > 0 cpus yet."
#endif
	return 0;
}

#endif /* CPUID_H */
