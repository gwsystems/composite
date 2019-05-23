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
#define MULTIPLE 100

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

#define ITERS 10

int main(void)
{
	unsigned long long max = 0;
	int i;
	unsigned long long x, y;

	rdtscll(x);
	__spin_fn();
	rdtscll(y);
	printc("%llu:%llu\n", y - x, sl_cyc2usec(y - x));


	for (i = 0; i < ITERS; i++) {
		volatile unsigned long long st = 0, en = 0;

		rdtscll(st);
		#pragma omp parallel
		{
			//printf("(a, %u:%u, %d)\n", sched_getcpu(), GETTID(), omp_get_thread_num());
			#pragma omp single
			{
				//printf("(b, %u:%u, %d)\n", sched_getcpu(), GETTID(), omp_get_thread_num());
				#pragma omp task
				{
					//printf("(c, %u:%u, %d)\n", sched_getcpu(), GETTID(), omp_get_thread_num());
					#pragma omp task
					{
						__spin_fn();
						//printf("(d, %u:%u, %d)\n", sched_getcpu(), GETTID(), omp_get_thread_num());
					}
					#pragma omp taskwait
				}

				#pragma omp task
				{
					__spin_fn();
					//printf("(e, %u:%u, %d)\n", sched_getcpu(), GETTID(), omp_get_thread_num());
				}
				__spin_fn();
				#pragma omp taskwait
			}
			//printf("(f, %u:%u, %d)\n", sched_getcpu(), GETTID(), omp_get_thread_num());
		}
		rdtscll(en);
		long diff = en - st;
		if (diff > 0) {
		       if (max < diff) max = diff;
			printc("%llu\n", (en - st) / CYC_US);
		}
	}

	printc("Max: %llu\n", max / CYC_US);
//	printf("Time: %llu, %llu\n", en - st, (en -st) / CYC_US);

	return 0;
}
