#include "spinlib.h"

#define CALIB 256

u64_t cycs_per_spin_iters = 0;
u64_t usecs_per_spin_iters = 0;

void spinlib_calib(void) __attribute__((optimize("O0")));
void spinlib_usecs(cycles_t usecs) __attribute__((optimize("O0")));
void spinlib_cycles(cycles_t cycs) __attribute__((optimize("O0")));
void spinlib_std_iters(void) __attribute__((optimize("O0")));

void
spinlib_std_iters(void)
{
	unsigned int i;

	for (i = 0 ; i < ITERS_SPIN ; i++) {
		__asm__ __volatile__("nop": : :"memory");
	}
}

/* time taken in that loop */
void
spinlib_calib(void)
{
	cycles_t total_cycs = 0;
	unsigned int iters = 0;

	while (iters < CALIB) {
		cycles_t start, end;

		rdtscll(start);
		spinlib_std_iters();
		rdtscll(end);

		total_cycs += (end - start);
		iters ++;
	}

	cycs_per_spin_iters = total_cycs / CALIB;
	usecs_per_spin_iters = cycs_per_spin_iters / cycs_per_usec;

	printc("Spin calibration: ITERS:%u Cycs/ITERS:%llu usecs/ITERS:%llu\n", 
	       ITERS_SPIN, cycs_per_spin_iters, usecs_per_spin_iters);
}

u64_t
cycles_per_spin_iters(void)
{ return cycs_per_spin_iters; }

u64_t
microsecs_per_spin_iters(void)
{ return usecs_per_spin_iters; }

unsigned
std_spin_iters(void)
{ return (unsigned)ITERS_SPIN; }

void
spinlib_cycles(cycles_t cycs)
{
	unsigned int i = 0;
	unsigned int iters = cycs / cycs_per_spin_iters;
	unsigned int left = cycs % cycs_per_spin_iters;

	assert(cycs >= cycs_per_spin_iters);

	/* round off to next cycs/spin */
	if (left >= (cycs_per_spin_iters / 2)) iters ++;

	while (i < iters) {
		spinlib_std_iters();
		i ++;
	}
}

void
spinlib_usecs(cycles_t usecs)
{
	unsigned int i = 0;
	unsigned int iters = usecs / usecs_per_spin_iters;
	unsigned int left = usecs % usecs_per_spin_iters;

	assert(usecs >= usecs_per_spin_iters);

	/* round off to next usec */
	if (left >= (usecs_per_spin_iters / 2)) iters ++;

	while (i < iters) {
		spinlib_std_iters();
		i ++;
	}
}
