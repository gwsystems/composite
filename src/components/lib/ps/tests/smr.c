#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <ps_smr.h>
#include <ps_plat.h>

struct parsec ps;
PS_PARSLAB_CREATE(tst, 100, PS_PAGE_SIZE * 128)
PS_PARSLAB_CREATE(bench, 1, PS_PAGE_SIZE * 8)

#define ITER 1024
void *ptrs[ITER];

void
test_mem(void)
{
	ps_tsc_t start, end;
	int i, j;

	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) ptrs[j] = ps_mem_alloc_tst();
	for (j = 0 ; j < ITER ; j++) ps_mem_free_tst(ptrs[j]);
	end = ps_tsc();
	end = (end-start)/ITER;
	printf("Average cost of alloc->free: %lld\n", end);

	ps_mem_alloc_tst();
	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < ITER ; i++) ptrs[i] = ps_mem_alloc_tst();
		for (i = 0 ; i < ITER ; i++) ps_mem_free_tst(ptrs[i]);
	}
	end = ps_tsc();
	end = (end-start)/(ITER*ITER);
	printf("Average cost of ITER * (alloc->free): %lld\n", end);

	printf("Starting complicated allocation pattern for increasing numbers of allocations.\n");
	for (i = 0 ; i < ITER ; i++) {
		ptrs[i] = ps_mem_alloc_tst();
		for (j = i+1 ; j < ITER ; j++) {
			ptrs[j] = ps_mem_alloc_tst();
		}
		for (j = i+1 ; j < ITER ; j++) {
			ps_mem_free_tst(ptrs[j]);
		}
	}
	for (i = 0 ; i < ITER ; i++) {
		assert(ptrs[i]);
		ps_mem_free_tst(ptrs[i]);
	}
}

void test_smr(void);
void test_remote_frees(void);

int
main(void)
{
	thd_set_affinity(pthread_self(), 0);

	printf("Starting tests on core %d.\n", ps_coreid());
	ps_init(&ps);
	ps_mem_init_tst(&ps);
	ps_mem_init_bench(&ps);

	printf("Testing memory management functionalities.\n");
 	test_mem();
	printf("Testing Scalable Memory Reclamation.\n");
	test_smr();
	printf("Testing remote frees\n");
	test_remote_frees();

	return 0;
}

#define N_OPS (50000000)
#define N_LOG (N_OPS / PS_NUMCORES)
static char ops[N_OPS] PS_ALIGNED;
static unsigned long results[PS_NUMCORES][2] PS_ALIGNED;
static unsigned long p99_log[N_LOG] PS_ALIGNED;

/* for qsort */
static int
cmpfunc(const void * a, const void * b)
{ return ( *(int*)a - *(int*)b ); }

void
bench(void)
{
	int i, id;
	unsigned long n_read = 0, n_update = 0, op_jump = PS_NUMCORES;
	unsigned long long s, e, s1, e1, tot_cost_r = 0, tot_cost_w = 0, max = 0, cost;
	void *last_alloc;

	id = ps_coreid();
	last_alloc = ps_mem_alloc_bench();
	assert(last_alloc);

	s = ps_tsc();
	for (i = 0 ; i < N_OPS/PS_NUMCORES; i++) {
		s1 = ps_tsc();

		if (ops[(unsigned long)id+op_jump*i]) {
			ps_mem_free_bench(ps_mem_alloc_bench());

			e1 = ps_tsc();
			cost = e1-s1;
			tot_cost_w += cost;
			n_update++;

			if (id == 0) p99_log[N_LOG - n_update] = cost;
		} else {
			ps_enter(&ps);
			ps_exit(&ps);

			e1 = ps_tsc();
			cost = e1-s1;
			tot_cost_r += cost;

			if (id == 0) p99_log[n_read] = cost;
			n_read++;
		}

		if (cost > max) max = cost;
	}
	assert(n_read + n_update <= N_LOG);
	e = ps_tsc();

	if (n_read)   tot_cost_r /= n_read;
	if (n_update) tot_cost_w /= n_update;

	results[id][0] = tot_cost_r;
	results[id][1] = tot_cost_w;

	if (id == 0) {
		unsigned long r_99 = 0, w_99 = 0;

		if (n_read) {
			qsort(p99_log, n_read, sizeof(unsigned long), cmpfunc);
			r_99 = p99_log[n_read - n_read / 100];
		}
		if (n_update) {
			qsort(&p99_log[n_read], n_update, sizeof(unsigned long), cmpfunc);
			w_99 = p99_log[N_LOG - 1 - n_update / 100];
		}
		printf("99p: read %lu write %lu\n", r_99, w_99);
	}


        printf("Thd %d: tot %lu ops (r %lu, u %lu) done, %llu (r %llu, w %llu) cycles per op, max %llu\n",
               id, n_read+n_update, n_read, n_update, (unsigned long long)(e-s)/(n_read + n_update),
               tot_cost_r, tot_cost_w, max);

	return;
}

char *TRACE_FILE = "/tmp/trace.dat";

void *
worker(void *arg)
{
	ps_tsc_t s,e;
	int cpuid = (int)(long)arg;

	thd_set_affinity(pthread_self(), cpuid);
	assert(!__ps_mem_bench.percore[cpuid].slab_info.fl.list);
	assert(ps_mem_alloc_bench());

	meas_barrier(PS_NUMCORES);
	s = ps_tsc();
	bench();
	e = ps_tsc();
	meas_barrier(PS_NUMCORES);

	if (cpuid == 0) {
		int i;
		unsigned long long tot_r = 0, tot_w = 0;

		for (i = 0; i < PS_NUMCORES; i++) {
			tot_r += results[i][0];
			tot_w += results[i][1];

			results[i][0] = 0;
			results[i][1] = 0;
		}
		tot_r /= PS_NUMCORES;
		tot_w /= PS_NUMCORES;

		printf("Summary: %s, (r %llu, w %llu) cycles per op\n", TRACE_FILE, tot_r, tot_w);
	}

	printf("cpu %d done in %llu cycles (%llu to %llu)\n", cpuid, e-s, s, e);

	return 0;
}

void
trace_gen(int fd, unsigned int nops, unsigned int percent_update)
{
	unsigned int i;

	srand(time(NULL));
	for (i = 0 ; i < nops ; i++) {
		char value;
		if ((unsigned int)rand() % 100 < percent_update) value = 'U';
		else                               value = 'R';
		if (write(fd, &value, 1) < 1) {
			perror("Writing to trace file");
			exit(-1);
		}
	}
	lseek(fd, 0, SEEK_SET);
}

void
load_trace(void)
{
	int fd, ret;
	int bytes;
	unsigned long i, n_read, n_update;

	ret = mlock(ops, N_OPS);
	if (ret) {
		printf("Cannot lock memory (%d). Check privilege (i.e. use sudo). Exit.\n", ret);
		exit(-1);
	}

	printf("loading trace file @ %s.\n", TRACE_FILE);
	/* read the entire trace into memory. */
	fd = open(TRACE_FILE, O_RDONLY);
	if (fd < 0) {
		fd = open(TRACE_FILE, O_CREAT | O_RDWR, S_IRWXU);
		assert(fd >= 0);
		trace_gen(fd, N_OPS, 50);
	}

	bytes = read(fd, &ops[0], N_OPS);
	assert(bytes == N_OPS);
	n_read = n_update = 0;

	for (i = 0 ; i < N_OPS ; i++) {
		if      (ops[i] == 'R') { ops[i] = 0; n_read++; }
		else if (ops[i] == 'U') { ops[i] = 1; n_update++; }
		else assert(0);
	}
	printf("Trace: read %lu, update %lu, total %lu\n", n_read, n_update, (n_read+n_update));
	assert(n_read+n_update == N_OPS);

	close(fd);

	return;
}

pthread_t thds[PS_NUMCORES];

void
test_smr(void)
{
	int i, ret;

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret) {
		printf("cannot lock memory %d... exit.\n", ret);
		exit(-1);
	}
	load_trace();

	for (i = 1 ; i < PS_NUMCORES ; i++) {
		ret = pthread_create(&thds[i], 0, worker, (void *)(long)i);
		if (ret) exit(-1);
	}
	usleep(50000);

	worker((void *)0);

	/* for (i = 1 ; i < PS_NUMCORES ; i++) { */
	for (i = PS_NUMCORES-1 ; i > 0 ; i--) {
		pthread_join(thds[i], (void *)&ret);
	}

	return;
}



#define RB_SZ   (1024 * 32)
#define RB_ITER (RB_SZ * 1024)

void * volatile ring_buffer[RB_SZ] PS_ALIGNED;

unsigned long long free_tsc, alloc_tsc;

void
consumer(void)
{
	char *s;
	unsigned long i;
	unsigned long long start, end, tot = 0;

	meas_barrier(2);

	for (i = 0 ; i < RB_ITER ; i++) {
		unsigned long off = i % RB_SZ;

		while (!ring_buffer[off]) ;
		s = ring_buffer[off];
		ring_buffer[off] = NULL;

		start = ps_tsc();
		ps_mem_free_bench(s);
		end = ps_tsc();
		tot += end-start;
	}
	free_tsc = tot / RB_ITER;
}

void
producer(void)
{
	char *s;
	unsigned long i;
	unsigned long long start, end, tot = 0;

	meas_barrier(2);

	for (i = 0 ; i < RB_ITER ; i++) {
		unsigned long off = i % RB_SZ;
		
		while (ring_buffer[off]) ; 

		start = ps_tsc();
		s = ps_mem_alloc_bench();
		end = ps_tsc();
		tot += end-start;

		assert(s);
		ring_buffer[off] = s;
	}
	alloc_tsc = tot / RB_ITER;
}

void *
child_fn(void *d)
{
	(void)d;
	
	thd_set_affinity(pthread_self(), 1);
	consumer();
	
	return NULL;
}

void
test_remote_frees(void)
{
	pthread_t child;
	
	printf("Starting test for remote frees\n");

	if (pthread_create(&child, 0, child_fn, NULL)) {
		perror("pthread create of child\n");
		exit(-1);
	}

	producer();

	pthread_join(child, NULL);
	printf("Remote allocations take %lld, remote frees %lld (unadjusted for tsc)\n", alloc_tsc, free_tsc);
}
