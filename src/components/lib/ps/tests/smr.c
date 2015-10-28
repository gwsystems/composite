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

struct parsec ps;
PS_PARSLAB_CREATE(tst, 8000, PS_PAGE_SIZE * 128)
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

int
main(void)
{
	printf("Starting tests on core %d.\n", ps_coreid());
	ps_init(&ps);
	ps_mem_init_tst(&ps);
	ps_mem_init_bench(&ps);

	printf("Testing memory management functionalities.\n");
	test_mem();
	printf("Testing Scalable Memory Reclamation.\n");
	test_smr();

	return 0;
}

struct thd_active {
	volatile int barrierval;
} CACHE_ALIGNED;

struct thd_active thd_active[PS_NUMCORES] PS_ALIGNED;

/* Only used in Linux tests. */
const int identity_mapping[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
const int *cpu_assign        = identity_mapping;
/* int cpu_assign[41] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, */
/* 		      1, 5, 9, 13, 17, 21, 25, 29, 33, 37, */
/* 		      2, 6, 10, 14, 18, 22, 26, 30, 34, 38, */
/* 		      3, 7, 11, 15, 19, 23, 27, 31, 35, 39, -1}; */

static void
call_getrlimit(int id, char *name)
{
	struct rlimit rl;
	(void)name;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: ");
		exit(-1);
	}
}

static void
call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		exit(-1);
	}
}

void
set_prio(void)
{
	struct sched_param sp;

	call_getrlimit(RLIMIT_CPU, "CPU");
#ifdef RLIMIT_RTTIME
	call_getrlimit(RLIMIT_RTTIME, "RTTIME");
#endif
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_getrlimit(RLIMIT_NICE, "NICE");

	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		exit(-1);
	}
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: ");
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		exit(-1);
	}
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

	return;
}

void
thd_set_affinity(pthread_t tid, int id)
{
	cpu_set_t s;
	int ret, cpuid;
	coreid_t cid, n;

	cpuid = cpu_assign[id];
	CPU_ZERO(&s);
	CPU_SET(cpuid, &s);

	ret = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &s);
	if (ret) {
		printf("setting affinity error for cpu %d\n", cpuid);
		assert(0);
	}

	/* set_prio(); */
	/* confirm that the library's version of coreid == benchmark's */
	ps_tsc_locality(&cid, &n);
	assert(cpuid == cid);
}

/*
 * Trivial barrier
 */
void
meas_barrier(void)
{
	int cpu = ps_coreid();
	int initval = thd_active[cpu].barrierval, doneval = !initval;

	if (cpu == 0) {
		int k;
		for (k = 1 ; k < PS_NUMCORES ; k++) {
			while (thd_active[k].barrierval == initval) ;
		}
		thd_active[0].barrierval = doneval;
	} else {
		thd_active[cpu].barrierval = doneval;
		while (thd_active[0].barrierval == initval) ;
	}
	/* gogogo! */
}

#define N_OPS (10000000)
#define N_LOG (N_OPS / PS_NUMCORES)
static char ops[N_OPS] PS_ALIGNED;
static unsigned long results[PS_NUMCORES][2] PS_ALIGNED;
static unsigned long p99_log[N_LOG] PS_ALIGNED;

/* for qsort */
static int
cmpfunc(const void * a, const void * b)
{
	return ( *(int*)a - *(int*)b );
}

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

	meas_barrier();
	s = ps_tsc();
	bench();
	e = ps_tsc();
	meas_barrier();

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
	char buf[PS_PAGE_SIZE+1];

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

	for (i = 0; i < (N_OPS / PS_PAGE_SIZE); i++) {
		bytes = read(fd, buf, PS_PAGE_SIZE);
		assert(bytes == PS_PAGE_SIZE);
		memcpy(&ops[i * PS_PAGE_SIZE], buf, bytes);
	}

	if (N_OPS % PS_PAGE_SIZE) {
		bytes = read(fd, buf, PS_PAGE_SIZE);
		memcpy(&ops[i*PS_PAGE_SIZE], buf, bytes);
	}
	n_read = n_update = 0;
	for (i = 0; i < N_OPS; i++) {
		if      (ops[i] == 'R') { ops[i] = 0; n_read++; }
		else if (ops[i] == 'U') { ops[i] = 1; n_update++; }
		else assert(0);
	}
	printf("Trace: read %lu, update %lu, total %lu\n", n_read, n_update, (n_read+n_update));
	assert(n_read+n_update == N_OPS);

	close(fd);

	return;
}

void
test_smr(void)
{
	int i, ret;
	pthread_t thds[PS_NUMCORES];

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret) {
		printf("cannot lock memory %d... exit.\n", ret);
		exit(-1);
	}

	thd_set_affinity(pthread_self(), 0);

	load_trace();

/* #ifndef SYNC_USE_RDTSC */
/* 	create_timer_thd(PS_NUMCORES-1); */
/* #endif */
	for (i = 1 ; i < PS_NUMCORES ; i++) {
		ret = pthread_create(&thds[i], 0, worker, (void *)(long)i);
		if (ret) exit(-1);
	}

	usleep(50000);

	worker((void *)0);

	for (i = 1 ; i < PS_NUMCORES ; i++) {
		pthread_join(thds[i], (void *)&ret);
	}

	return;
}
