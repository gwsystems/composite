struct thd_active {
	int accessed;
	int done;
	int avg;
	int max;
	int read_avg;
	int read_max;
} CACHE_ALIGNED;

struct thd_active thd_active[NUM_CPU] CACHE_ALIGNED;
volatile int use_ncores;

#ifdef IN_LINUX

/* Only used in Linux tests. */
/* int cpu_assign[5] = {0, 1, 2, 3, -1}; */
int cpu_assign[41] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		      1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
		      2, 6, 10, 14, 18, 22, 26, 30, 34, 38,
		      3, 7, 11, 15, 19, 23, 27, 31, 35, 39, -1};

static void call_getrlimit(int id, char *name)
{
	struct rlimit rl;
	(void)name;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: ");
		exit(-1);
	}		
}

static void call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		exit(-1);
	}		
}

void set_prio(void)
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

void thd_set_affinity(pthread_t tid, int id)
{
	cpu_set_t s;
	int ret, cpuid;

	cpuid = cpu_assign[id];
	/* printf("tid %d (%d) to cpu %d\n", tid, id, cpuid); */
	CPU_ZERO(&s);
	CPU_SET(cpuid, &s);

	ret = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &s);

	if (ret) {
//		printf("setting affinity error for cpu %d\n", cpuid);
		assert(0);
	}
	set_prio();
}

#endif

void meas_sync_start(void) {
	int cpu = get_cpu();
	ck_pr_store_int(&thd_active[cpu].done, 0);
	ck_pr_store_int(&thd_active[cpu].avg, 0);
	ck_pr_store_int(&thd_active[cpu].max, 0);
	ck_pr_store_int(&thd_active[cpu].read_avg, 0);
	ck_pr_store_int(&thd_active[cpu].read_max, 0);

	if (cpu == 0) {
		int k = 1;
		while (k < use_ncores) {
			while (1) {
				if (ck_pr_load_int(&thd_active[k].accessed)) break;
			}
			k++;
		}
		ck_pr_store_int(&thd_active[0].accessed, 1);
	} else {
		ck_pr_store_int(&thd_active[cpu].accessed, 1);
		while (ck_pr_load_int(&thd_active[0].accessed) == 0) ;
	} // sync!
}

void meas_sync_end() {
	int i;
	int cpu = get_cpu();
	ck_pr_store_int(&thd_active[cpu].accessed, 0);

	if (cpu == 0) { // output!!!
		// sync first!
		for (i = 1; i < use_ncores;i++) {
			while (1) {
				if (ck_pr_load_int(&thd_active[i].done)) break;
			}
		}

		ck_pr_store_int(&thd_active[0].done, 1);
	} else {
		ck_pr_store_int(&thd_active[cpu].done, 1);
		while (ck_pr_load_int(&thd_active[0].done) == 0) ;
	}
}

void parsec_init(parsec_t *parsec)
{
	int i;
	struct percpu_info *percpu;
	
	memset(parsec, 0, sizeof(struct parsec));

	/* mark everyone as not in the section. */
	for (i = 0; i < NUM_CPU; i++) {
		percpu = &(parsec->timing_info[i]);
		percpu->timing.time_in = 0;
		percpu->timing.time_out = 1;
	}

	return;
}

#ifdef IN_LINUX
#define ITEM_SIZE (CACHE_LINE)

void parsec_init(void)
{
	int i, ret;

	use_ncores = NUM_CPU;

	allmem = malloc(MEM_SIZE + PAGE_SIZE);
	/* adjust to page aligned. */
	quie_mem = allmem + (PAGE_SIZE - (unsigned long)allmem % PAGE_SIZE); 
	assert((unsigned long)quie_mem % PAGE_SIZE == 0);

	ret = mlock(allmem, MEM_SIZE + PAGE_SIZE);
	if (ret) {
//		printf("Cannot lock cache memory (%d). Check privilege. Exit.\n", ret);
		exit(-1);
	}
	memset(allmem, 0, MEM_SIZE + PAGE_SIZE);

	for (i = 0; i < QUIE_QUEUE_N_SLAB; i++) {
		ck_spinlock_init(&glb_freelist.slab_freelists[i].l);
	}

	for (i = 0; i < NUM_CPU; i++) {
		timing_info[i].timing.time_in  = 0;
		timing_info[i].timing.time_out = 1;
	}

	mem_warmup();

	/* printf("\n << PARSEC: init done, cache memory %d MBs. Quiescence queue size %d >>> \n",  */
	/*        MEM_SIZE >> 20, QUIE_QUEUE_LIMIT); */

	return;
}
#endif
