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
#include "shared/cos_types.h"
#include "spd.h"
#include "debug.h"
#include "cpuid.h"

#define GET_CURR_CPU get_cpuid()

struct per_core_variables {
	struct thread *curr_thd;
	struct spd_poly *curr_spd;
} CACHE_ALIGNED;

extern struct per_core_variables per_core[NUM_CPU];

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
