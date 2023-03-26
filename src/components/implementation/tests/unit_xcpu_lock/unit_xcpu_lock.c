/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <sync_lock.h>
#include <perfdata.h>
#include <cos_time.h>

#undef LOCK_TRACE_DEBUG
#ifdef LOCK_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

#define ITERATION 10

struct sync_lock lock;
volatile unsigned long long cnt;
volatile int res[NUM_CPU] = { 0 };
volatile int done = 0;

void
simple_spin(unsigned long long iter)
{
	cnt = 0;
	while (cnt < iter * 10000000) cnt++;
}

void
print_res(void)
{
	int i = 0;

	for (i =0; i < NUM_CPU; i++) {
		printc("CPU%d take the lock %d times\n", i, res[i]);
	}
}

void
contention_lock(void)
{
	int i = 0;
	while (!done) {
		sync_lock_take(&lock);
		res[cos_cpuid()] ++;
		simple_spin((unsigned long long)50);
		sync_lock_release(&lock);
		simple_spin((unsigned long long)100);
		i++;
		if (cos_cpuid() == 0 && i >= ITERATION) {
			done = 1;
			printc("SUCCESS:\n");
			print_res();
		}
	}

	return;
}

void
cos_init()
{
	sync_lock_init(&lock);
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{
	contention_lock();

	return;
}
