#ifndef SPINLIB_H
#define SPINLIB_H

#include <cos_debug.h>
#include <cos_types.h>
#include <cos_component.h>

/*
 * this is probably the trickiest thing to configure and
 * the accuracy of the workgen depends very much on this.
 */
#define SPINLIB_ITERS_SPIN (618)

extern unsigned int spinlib_cycs_per_us;

extern void spinlib_calib(unsigned int cycs_per_us);
extern void spinlib_usecs(cycles_t usecs);
extern void spinlib_cycles(cycles_t cycs);

#endif /* SPINLIB_H */
