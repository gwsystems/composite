/**
 * Copyright 2012 by Qi Wang, interwq@gwu.edu; Gabriel Parmer,
 * gparmer@gwu.edu

 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CPUID_H
#define CPUID_H

#include "shared/consts.h"

/* TODO: put this in platform specific directory */
static inline unsigned long *
get_linux_thread_info(void)
{
	unsigned long curr_stk_pointer;
	asm ("movl %%esp, %0;" : "=r" (curr_stk_pointer));
	return (unsigned long *)(curr_stk_pointer & ~(THREAD_SIZE_LINUX - 1));
}

static inline unsigned int
get_cpuid(void)
{
#if NUM_CPU == 1
	return 0;
#endif
	/* Linux saves the CPU_ID in the stack for fast access. */
	return *(get_linux_thread_info() + CPUID_OFFSET_IN_THREAD_INFO);
}

#endif /* CPUID_H */
