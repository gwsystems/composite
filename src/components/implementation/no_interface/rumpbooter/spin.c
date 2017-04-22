#include "spin.h"

#define CALIB 256

u64_t cycs_per_spin_iters = 0;
u64_t usecs_per_spin_iters = 0;
u64_t spin_calib(void) __attribute__((optimize("O0")));
void spin_usecs(cycles_t usecs) __attribute__((optimize("O0")));
void spin_cycles(cycles_t cycs) __attribute__((optimize("O0")));
void spin_std_iters(void) __attribute__((optimize("O0")));

/* time taken in that loop */
u64_t
spin_calib(void)
{
	cycles_t total_cycs = 0;
	unsigned int iters = 0;

	while (iters < CALIB) {
		cycles_t start, end;
		u64_t spin = 0;

		rdtscll(start);
		while (spin < (u64_t)ITERS_SPIN) spin ++;
		rdtscll(end);

		total_cycs += (end - start);
		iters ++;
	}

	cycs_per_spin_iters = total_cycs / CALIB;
	usecs_per_spin_iters = cycs_per_spin_iters / cycs_per_usec;

	return cycs_per_spin_iters;
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
spin_std_iters(void)
{
	unsigned int i = 0;

	while (i < ITERS_SPIN) i ++;
}

void
spin_cycles(cycles_t cycs)
{
	unsigned int i = 0;
	unsigned int iters = cycs / cycs_per_spin_iters;
	unsigned int left = cycs % cycs_per_spin_iters;

	if (cycs < cycs_per_spin_iters) return;
	//assert(cycs >= cycs_per_spin_iters);
	/* round off to next cycs/spin */
	if (left >= (cycs_per_spin_iters / 2)) iters ++;

	while (i < iters) {
		spin_std_iters();
		i ++;
	}
}

void
spin_usecs(cycles_t usecs)
{
	unsigned int i = 0;
	unsigned int iters = usecs / usecs_per_spin_iters;
	unsigned int left = usecs % usecs_per_spin_iters;

	if (usecs < usecs_per_spin_iters) return;
	//assert(usecs >= usecs_per_spin_iters);
	/* round off to next usec */
	if (left >= (usecs_per_spin_iters / 2)) iters ++;

	while (i < iters) {
		spin_std_iters();
		i ++;
	}
}
