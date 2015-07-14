/**
 * Copyright 2012 by Qi Wang, interwq@gwu.edu; Gabriel Parmer,
 * gparmer@gwu.edu

 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CPUID_H
#define CPUID_H

#include "../../../../kernel/include/shared/consts.h"
#include "../../../../kernel/include/asm_ipc_defs.h"

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

static inline void *
get_linux_thread_info(void)
{
	unsigned long curr_stk_pointer;
	asm ("movl %%esp, %0;" : "=r" (curr_stk_pointer));
	return (void *)(curr_stk_pointer & LINUX_INFO_PAGE_MASK);
}

#ifndef LINUX_TEST
static inline struct cos_cpu_local_info *
cos_cpu_local_info(void)
{
	return (struct cos_cpu_local_info *)(get_linux_thread_info() + LINUX_THREAD_INFO_RESERVE);
}
#else

//LINUX user space test case. 
struct cos_cpu_local_info local_info;
static inline struct cos_cpu_local_info *
cos_cpu_local_info(void)
{
	return &local_info;
}
#endif

static inline int
get_cpuid(void)
{
#if NUM_CPU > 1
	/* Here, we get cpuid info from linux structure instead of Cos
	 * structure. This enables linux tasks to get the correct
	 * cpuid with this function, which is useful when doing
	 * timer_interposition.*/

	/* We save the CPU_ID in the stack for fast access. */
	return *(unsigned long *)(get_linux_thread_info() + CPUID_OFFSET_IN_THREAD_INFO);
#else 
	/* Optimize for the uniprocessor case. */
	return 0;
#endif
}

#endif /* CPUID_H */
