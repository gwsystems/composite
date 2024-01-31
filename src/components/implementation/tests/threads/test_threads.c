#include <llprint.h>
#include <res_spec.h>
#include <sched.h>
#include <cos_time.h>
#include <initargs.h>
#include <cos_trace.h>
#include <string.h>

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)
#define HIGHEST_PRIORITY 0

static void
workload(unsigned long long loop_count)
{
	unsigned long long i, j;

    for (i = 0; i < loop_count; ++i) {
        for (j = 0; j < 100; ++j) {
            __asm__ volatile(""); // Prevents the compiler from optimizing the loop away
        }
    }
}

static void
spinning_task()
{
	SPIN();
}

static void
blocking_task(cycles_t blocking_time)
{
	cycles_t blocking_period = blocking_time;

	while (1) {
		// Hacked to find a good value, gonna change it later
		for (cycles_t i = 0; i < 9964000; i++) {
			sched_thd_block_timeout(0, time_now() + i);
			workload(27000);
		}
	}
}

cycles_t
measure_cycles(unsigned long long loop_count)
{
	unsigned cycles_high, cycles_low, cycles_high1, cycles_low1;

	__asm__ __volatile__("cpuid\n\t" 
						 "rdtsc\n\t" 
						 "mov %%edx, %0\n\t" 
						 "mov %%eax, %1\n\t" : 
						 "=r" (cycles_high), "=r" (cycles_low) :: "%rax", "%rbx", "%rcx", "%rdx");

	workload(loop_count);

	__asm__ __volatile__("rdtscp\n\t" 
						 "mov %%edx, %0\n\t" 
						 "mov %%eax, %1\n\t" 
						 "cpuid\n\t" : 
						 "=r" (cycles_high1), "=r" (cycles_low1) :: "%rax", "%rbx", "%rcx", "%rdx");


	cycles_t start = (((cycles_t)cycles_high << 32) | cycles_low);
	cycles_t end = (((cycles_t)cycles_high1 << 32) | cycles_low1);

	PRINTLOG(PRINT_DEBUG, "Loop Count: %llu Start: %llu, End: %llu, Diff: %llu\n",loop_count , start, end, end - start);

	return end - start;
}

cycles_t
find_loop_count(cycles_t desired_execution_time)
{
	unsigned long long loop_count_min = 25000; // For 1 ms measured loop count is ~25900
	unsigned long long loop_count = 0;

	for (unsigned long long i = loop_count_min; i < desired_execution_time; i++)
	{
		cycles_t cost = 0;
		for (int j = 0; j < 1000; j++)
		{
			cost += measure_cycles(i);
		}
		cost /= 1000;

		// If it is in error margin, break
		//if (cost > desired_execution_time - 2000 && cost < desired_execution_time + 2000)
		if (cost >= desired_execution_time)	{
			loop_count = i;
			PRINTLOG(PRINT_DEBUG, "Loop count: %llu, Cycles: %llu\n", i, cost);
			break;
		}

	}

	// Measure it again with the new loop count
	cycles_t avg2 = 0;
	for (size_t i = 0; i < 1000; i++)
	{
		avg2 += measure_cycles(loop_count);

	}
	avg2 /= 1000;

	PRINTLOG(PRINT_DEBUG, "Average cycles: %llu Desired cycles: %llu\n", avg2, desired_execution_time);

	return loop_count;
}


enum thd_type_t {
	SPINNER = 0,
	BLOCKING = 1, // TODO: Add more types
};

struct thread_props {
	enum thd_type_t type;
	thdid_t tid;
	int priority;
	int budget_us;
	int period_us;
	int execution_us;
	int block_us;
};

thdid_t
create_thread(const char* args)
{
	char *token;
	struct thread_props thd;

	int result = sscanf(args, "%d,%d,%d,%d,%d,%d", (int*)&thd.type, &thd.priority, &thd.period_us, &thd.budget_us, &thd.execution_us, &thd.block_us);
    
	if (result != 6) {
        PRINTLOG(PRINT_DEBUG, "Parsing failed\n");
        return 1;
    }

	assert (thd.priority >= HIGHEST_PRIORITY && thd.priority <= LOWEST_PRIORITY);
	assert(thd.period_us > thd.budget_us);

	switch (thd.type)
	{
	case SPINNER:
	{
		thd.tid = sched_thd_create(spinning_task, NULL);
		break;
	}
	case BLOCKING:
	{
		// TODO: blocking time is hardcoded for now
		cycles_t blocking_time = time_usec2cyc(2 * thd.period_us - thd.budget_us - 50000); // Error ??
		// TODO: Loop count is hardcoded for now
		cycles_t loop_count = find_loop_count(time_usec2cyc(thd.execution_us));
		thd.tid = sched_thd_create(blocking_task, &blocking_time);
		break;
	}
	/* TODO
	case MEASURED:
	{
		cycles_t loop_count = find_loop_count(time_usec2cyc(thd.execution_us));
		thd.tid = sched_thd_create(measured_task, &args);
		break;
	}
	*/
	default:
		assert(0);
		break;
	}

	PRINTLOG(PRINT_DEBUG, "\t TID: %lu, Type: %d, Priority: %d, Period: %d, Budget: %d\n", thd.tid, thd.type, thd.priority, thd.period_us, thd.budget_us);

	sched_thd_param_set(thd.tid, sched_param_pack(SCHEDP_PRIO, thd.priority));
	sched_thd_param_set(thd.tid, sched_param_pack(SCHEDP_BUDGET, time_usec2cyc(thd.budget_us)));
	sched_thd_param_set(thd.tid, sched_param_pack(SCHEDP_WINDOW, time_usec2cyc(thd.period_us)));
	
	return thd.tid;
}

int
main(void)
{
	int num_of_threads = 5; // To prevent dynamic allocation, just 
	thdid_t thread_ids[num_of_threads];

	struct initargs params, curr;
	struct initargs_iter i;
	char *token;
	int ret = 0;

	ret = args_get_entry("param", &params);
	assert(!ret);
	assert(args_len(&params) < num_of_threads);

	num_of_threads = args_len(&params);

	int num = 0;
	for (ret = args_iter(&params, &i, &curr) ; ret ; ret = args_iter_next(&i, &curr)) {
		int      keylen;
		char    *thread_args = args_value(&curr);
		assert(thread_args);
		thread_ids[num++] = create_thread(thread_args);
	}

	cycles_t time_before_wakeup = time_now();
	cycles_t sleep = time_usec2cyc(10000000);

	PRINTLOG(PRINT_DEBUG, "Starting %d threads\n", num_of_threads);
	sched_thd_block_timeout(0, time_before_wakeup + sleep);

	cycles_t wakeup = time_now();
	cycles_t spent = wakeup - time_before_wakeup;
	PRINTLOG(PRINT_DEBUG, "Time spent: %llu	\n", time_cyc2usec(spent));

	// Block test threads
	/* TODO: Blockedded for testing purposes, remove it later! */
	for(int i = 0; i < num_of_threads; i++) {
		// TODO: When it is used with blocking_task, it crashes fix it
		sched_thd_block(thread_ids[i]);
		PRINTLOG(PRINT_DEBUG, "\t TID: %lu blocked\n", thread_ids[i]);
	}


	/*
	cycles_t sched_exec_time, idle_exec_time = 0;
	sched_exec_time = (cycles_t)sched_thd_get_param(1, 0);
	idle_exec_time = (cycles_t)sched_thd_get_param(0, 0);
	*/

	// Print the total execution time of the tasks
	u16_t	 switch_cnt = 0;
	for(int i = 0; i < num_of_threads; i++) {
		cycles_t exec_time = 0;
		exec_time = (cycles_t)sched_thd_get_param(thread_ids[i], 0);
		switch_cnt = (u16_t)sched_thd_get_param(thread_ids[i], 1);
		PRINTLOG(PRINT_DEBUG, "Thdid: %lu Total exec time: %llu, Switch count: %u\n", thread_ids[i], exec_time, switch_cnt);
	}
	PRINTLOG(PRINT_DEBUG, "Test Finished\n");

	cos_trace_print_buffer();
	//sched_thd_block(0); 		

	return 0;
}
