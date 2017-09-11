#ifndef SPINLIB_H
#define SPINLIB_H

#include <cos_debug.h>
#include <cos_types.h>
#include <cos_component.h>
#include "cos2rk_types.h"

#define ITERS_SPIN (40000) //100usecs on my test machine. lets see.

extern u64_t cycs_per_spin_iters;
extern u64_t usecs_per_spin_iters;
extern void spinlib_calib(void);
extern void spinlib_usecs(cycles_t usecs);
extern void spinlib_cycles(cycles_t cycs);
extern void spinlib_std_iters(void);

#endif /* SPINLIB_H */
