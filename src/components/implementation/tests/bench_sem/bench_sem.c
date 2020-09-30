/*
 * Copyright 2016, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <sched.h>

#include <crt_sem.h>

#undef SEM_TRACE_DEBUG
#ifdef SEM_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* One low-priority thread and one high-priority thread contends on the semaphore */
#define ITERATION 10000

struct crt_sem sem;
thdid_t sem_hi = 0, sem_lo = 0;
volatile int flag = 0;

volatile cycles_t start;
volatile cycles_t end;

cycles_t results[ITERATION] = {0, };

cycles_t
average(cycles_t* array, int num)
{
	int i;
	cycles_t sum = 0;
	
	for (i = 0; i < num; i++) sum += array[i];
	
	return sum / num;
}

/***
 * The high priority thread periodically challenges the sem while the low priority thread keeps spinning.
 * When the low-priority thread detects that the flag is changed, it knows that the sem is challenged.
 * Execution: hi take -> lo release -> hi release, there is 1 contended take, 1 give, and 2 ctxsws. 
 */
void
sem_hi_thd(void *d)
{
	/* Never stops running; low priority controls how many iters to run. */
	while (1) {
		debug("h1");
		sched_thd_block(0);
		sched_thd_block_timeout(0, time_now() + time_usec2cyc(1000));
		
		debug("h2");
		flag = 1;
		start = time_now();
		
		crt_sem_take(&sem);
		crt_sem_give(&sem);

		end = time_now();
		debug("h3");
	}
}

void
sem_lo_thd(void *d)
{
	int i;

	for (i = 0; i < ITERATION; i++) {
		debug("l1");
		sched_thd_wakeup(sem_hi);

		debug("l2");
		flag = 0;
		crt_sem_take(&sem);
		
		debug("l3");
		while (flag != 1) {}
		
		crt_sem_give(&sem);
		
		results[i]=end-start;
		debug("l4");
	}
	
	printc("Contended: take+give %lld\n", average(results, ITERATION));

	while (1) ;
}

void
test_sem(void)
{
	int i;
	
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};

	crt_sem_init(&sem, 1);
	
	/* Uncontended semaphore taking/releasing */
	for (i = 0; i < ITERATION; i++) {
		start = time_now();
		
		crt_sem_take(&sem);
		crt_sem_give(&sem);

		end = time_now();
		results[i] = end - start;
	}
	
	printc("Uncontended: take+release %lld\n", average(results, ITERATION));

	memset(results, 0, sizeof(results));

	printc("Create threads:\n");
	
	sem_lo = sched_thd_create(sem_lo_thd, NULL);
	printc("\tcreating lo thread %d at prio %d\n", sem_lo, sps[1]);
	sched_thd_param_set(sem_lo, sps[1]);
	
	sem_hi = sched_thd_create(sem_hi_thd, NULL);
	printc("\tcreating hi thread %d at prio %d\n", sem_hi, sps[0]);
	sched_thd_param_set(sem_hi, sps[0]);
}

void
cos_init(void)
{
	printc("Benchmark for the crt_sem (w/sched interface).\n");
}

int
main(void)
{
	test_sem();

	printc("Running benchmark, exiting main thread...\n");

	return 0;
}
