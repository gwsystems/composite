#include "spinlib.h"
#include <sl.h>

#define SPINLIB_CALIB 256

static u64_t spinlib_cycs_per_spin_iters = 0;
static u64_t spinlib_usecs_per_spin_iters = 0;
unsigned int spinlib_cycs_per_us = 0;
static unsigned int spinlib_init = 0;

void spinlib_calib(unsigned int cycs_per_us) __attribute__((optimize("O0")));
void spinlib_usecs(cycles_t usecs) __attribute__((optimize("O0")));
void spinlib_cycles(cycles_t cycs) __attribute__((optimize("O0")));
void spinlib_std_iters(void) __attribute__((optimize("O0")));

#define SPINLIB_TEST_NITEMS 4

static void
spinlib_calib_test(void)
{
	microsec_t test_us[SPINLIB_TEST_NITEMS] = { 1000, 2000, 3000, 4000 };
	int i;

	for (i = 0; i < SPINLIB_TEST_NITEMS; i++) {
		cycles_t st, end, elapsed_cycs;

		rdtscll(st);
		spinlib_usecs(test_us[i]);
		rdtscll(end);
		elapsed_cycs = end - st;

		PRINTC("SPIN %lluus => elapsed :%lluus %llucycs\n", test_us[i], elapsed_cycs, sl_cyc2usec(elapsed_cycs));
	}
}

void
spinlib_std_iters(void)
{
	unsigned int i;

	for (i = 0 ; i < SPINLIB_ITERS_SPIN ; i++) {
		__asm__ __volatile__("nop": : :"memory");
	}
}

/* time taken in that loop */
void
spinlib_calib(unsigned int cycs_per_us)
{
	cycles_t total_cycs = 0;
	unsigned int iters = 0;

	if (spinlib_init) return;
	spinlib_cycs_per_us = cycs_per_us;

	while (iters < SPINLIB_CALIB) {
		cycles_t start, end;

		rdtscll(start);
		spinlib_std_iters();
		rdtscll(end);

		total_cycs += (end - start);
		iters ++;
	}

	spinlib_cycs_per_spin_iters = total_cycs / SPINLIB_CALIB;
	spinlib_usecs_per_spin_iters = spinlib_cycs_per_spin_iters / spinlib_cycs_per_us;

	spinlib_init = 0;
	printc("Spin calibration: ITERS:%u Cycs/ITERS:%llu usecs/ITERS:%llu\n",
	       SPINLIB_ITERS_SPIN, spinlib_cycs_per_spin_iters, spinlib_usecs_per_spin_iters);
	spinlib_calib_test();
}

void
spinlib_cycles(cycles_t cycs)
{
//	unsigned int i = 0;
//	unsigned int iters = cycs / spinlib_cycs_per_spin_iters;
//	unsigned int left = cycs % spinlib_cycs_per_spin_iters;
//
//	assert(cycs >= spinlib_cycs_per_spin_iters);
//
//	/* round off to next cycs/spin */
//	if (left >= (spinlib_cycs_per_spin_iters / 2)) iters ++;
//
//	while (i < iters) {
//		spinlib_std_iters();
//		i ++;
//	}
	unsigned long long st, en;

	rdtscll(st);
	en = st + cycs;

	// doesn't work with concurrency.. but don't care for now.
	do {
		rdtscll(st);
	} while (st < en);
}

void
spinlib_usecs(cycles_t usecs)
{
	unsigned long long cycs = sl_usec2cyc(usecs);

	spinlib_cycles(cycs);
//	unsigned int i = 0;
//	unsigned int iters = usecs / spinlib_usecs_per_spin_iters;
//	unsigned int left = usecs % spinlib_usecs_per_spin_iters;
//
//	assert(usecs >= spinlib_usecs_per_spin_iters);
//
//	/* round off to next usec */
//	if (left >= (spinlib_usecs_per_spin_iters / 2)) iters ++;
//
//	while (i < iters) {
//		spinlib_std_iters();
//		i ++;
//	}
}
