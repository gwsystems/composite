#define _GNU_SOURCE
#include <stdlib.h>
#include <sched.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sys/types.h>

#include <ps_plat.h>

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
		perror("setting affinity error\n");
		exit(-1);
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
meas_barrier(int ncores)
{
	int cpu = ps_coreid();
	int initval = thd_active[cpu].barrierval, doneval = !initval;

	if (cpu == 0) {
		int k;
		for (k = 1 ; k < ncores ; k++) {
			while (thd_active[k].barrierval == initval) ;
		}
		thd_active[0].barrierval = doneval;
	} else {
		thd_active[cpu].barrierval = doneval;
		while (thd_active[0].barrierval == initval) ;
	}
	/* gogogo! */
}
