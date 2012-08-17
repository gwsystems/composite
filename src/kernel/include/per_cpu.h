/**
 * Copyright 2012 by Qi Wang, interwq@gwu.edu; Gabriel Parmer,
 * gparmer@gwu.edu

 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef PER_CPU_H
#define PER_CPU_H

#include "shared/cos_config.h"
#include "shared/consts.h"
#include "spd.h"
#include "debug.h"


struct per_core_variables {
	struct thread *curr_thd;
	struct spd_poly *curr_spd;
} CACHE_ALIGNED;

extern struct per_core_variables per_core[NUM_CPU];

typedef int cpuid_t;

static inline int 
cos_cas(unsigned long *target, unsigned long cmp, unsigned long updated)
{
	char z;
	__asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (cmp)
			     : "memory", "cc");
	return (int)z;
}

/* TODO: put this in platform specific directory */
static inline unsigned long *
get_linux_thread_info(void)
{
	unsigned long curr_stk_pointer;
	asm ("movl %%esp, %0;" : "=r" (curr_stk_pointer));
	return (unsigned long *)(curr_stk_pointer & ~(THREAD_SIZE_LINUX - 1));
}

static inline u32_t
get_cpuid(void)
{
#if NUM_CPU == 1
	return 0;
#endif
	/* Linux saves the CPU_ID in the stack for fast access. */
	return *(get_linux_thread_info() + CPUID_OFFSET_IN_THREAD_INFO);
}

#define CREATE_PERCPU_FNS(type, name)           \
static inline type                              \
core_get_##name(void)		                \
{                                               \
	return per_core[get_cpuid()].name;      \
}                                               \
                                                \
static inline type                              \
core_get_##name##_id(cpuid_t core)              \
{                                               \
	return per_core[core].name;             \
}                                               \
                                                \
static inline void                              \
core_put_##name(type val)                       \
{                                               \
	per_core[get_cpuid()].name = val;       \
}                                               \
                                                \
static inline void                              \
core_put_##name##_id(cpuid_t core, type val)    \
{                                               \
	per_core[core].name = val;              \
}

CREATE_PERCPU_FNS(struct thread *, curr_thd); /* core_get/put_curr_thd */
CREATE_PERCPU_FNS(struct spd_poly *, curr_spd); /* core_get/put_curr_spd */

#endif /* PER_CPU_H */
