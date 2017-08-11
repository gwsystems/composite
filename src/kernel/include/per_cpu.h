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
#include "shared/util.h"
#include "chal/cpuid.h"

#define GET_CURR_CPU get_cpuid()

/* The following functions gets per_cpu info from kernel stack. */
#define CREATE_PERCPU_FNS(type, name)                                                          \
	static inline type cos_get_##name(void) { return (type)(cos_cpu_local_info()->name); } \
	static inline void cos_put_##name(type val) { cos_cpu_local_info()->name = (void *)(val); }

CREATE_PERCPU_FNS(struct thread *, curr_thd); /* cos_get/put_curr_thd */
// CREATE_PERCPU_FNS(struct spd_poly *, curr_spd);

/*********************************************************************/
/* The following approach uses per_cpu variables. Accessing them
 * touches one more page potentially. We get/save such info on stack
 * to avoid this. */
struct per_core_variables {
	struct thread *  curr_thd;
	struct spd_poly *curr_spd;
} CACHE_ALIGNED;

/* extern struct per_core_variables per_core[NUM_CPU]; */

#define CREATE_PERCPU_VAR_FNS(type, name)                                                     \
	static inline type core_get_##name(void) { return per_core[get_cpuid()].name; }       \
                                                                                              \
	static inline type core_get_##name##_id(cpuid_t core) { return per_core[core].name; } \
                                                                                              \
	static inline void core_put_##name(type val) { per_core[get_cpuid()].name = val; }    \
                                                                                              \
	static inline void core_put_##name##_id(cpuid_t core, type val) { per_core[core].name = val; }

/* Not used for now. */
// CREATE_PERCPU_VAR_FNS(struct thread *, curr_thd); /* core_get/put_curr_thd */
// CREATE_PERCPU_VAR_FNS(struct spd_poly *, curr_spd); /* core_get/put_curr_spd */

#endif /* PER_CPU_H */
