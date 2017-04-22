#ifndef SPIN_H
#define SPIN_H

#include <cos_debug.h>
#include <cos_types.h>
#include <cos_component.h>
#include "vk_types.h"

#define ITERS_SPIN (2*166666) //1000usecs on my test machine. lets see.

extern u64_t cycs_per_spin_iters;
extern u64_t usecs_per_spin_iters;
extern u64_t spin_calib(void);
extern void spin_usecs(cycles_t usecs);
extern void spin_cycles(cycles_t cycs);
extern void spin_std_iters(void);

#endif /* SPIN_H */
