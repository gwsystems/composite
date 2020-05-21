#include <assert.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <omp.h>
#include <llprint.h>
#include <cos_component.h>
#include <sl.h>

#define GETTID() cos_thdid()
#define sched_getcpu() cos_cpuid()
#define CYC_US 3200

/*
 * From Chaos tests!
 * NOTE: number obtained by running composite instance with no interference..
 *       (validated with fiasco so far, it is 10us)
 */
#define ITERS_10US 5850
#define MULTIPLE 10000

#define SPIN_ITERS (ITERS_10US*MULTIPLE)

static void __spin_fn(void) __attribute__((optimize("O0")));

static void
__spin_fn(void)
{
        unsigned int spin = 0;

        while (spin < SPIN_ITERS) {
                __asm__ __volatile__("nop": : :"memory");
                spin++;
        }
}

#define ITERS 1000

int main(void)
{
	unsigned long long max = 0, total = 0;
	int i;
	unsigned long long x, y;

	rdtscll(x);
	__spin_fn();
	rdtscll(y);
	printc("%llu:%llu\n\n\n", y - x, sl_cyc2usec(y - x));


	for (i = 0; i < ITERS; i++) {
		volatile unsigned long long st = 0, en = 0;

		rdtscll(st);
		#pragma omp parallel
		{
			#pragma omp single
			{
				#pragma omp task
				{
					#pragma omp task
					{
						__spin_fn();
					}
					#pragma omp taskwait
				}

				#pragma omp task
				{
					__spin_fn();
				}
				__spin_fn();
				#pragma omp taskwait
			}
		}
		rdtscll(en);
		long diff = en - st;
		assert(diff > 0);

		total += diff;
		if ((unsigned long long) diff > max) max = diff;
		printc("%ld, %ld\n", diff, diff / CYC_US);
	}

	printc("(cyc) Avg: %llu, Max: %llu\n", (total / ITERS), max);
	printc("(us) Avg: %llu, Max: %llu\n", (total / ITERS) / CYC_US, max / CYC_US);

	return 0;
}
