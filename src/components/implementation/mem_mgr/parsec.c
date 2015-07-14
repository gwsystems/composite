/* PARSEC library implementation */
#include "parsec.h"

//#include <ck_pr.h>

#ifdef IN_LINUX
#include <sys/mman.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

__thread int thd_local_id;

#endif

/* # of cachelines allocated. */
volatile unsigned long n_cacheline = 0;

static inline void 
quie_queue_fill(struct quie_queue *queue, size_t size, struct parsec_allocator *alloc)
{
	int i, diff;
	struct quie_mem_meta *meta;
	struct glb_freelist_slab *glb_freelist;

	glb_freelist = &alloc->glb_freelist;

	/* try freelist first -- the items are added to the
	 * quie waiting queue. */

	glb_freelist_get(queue, size, alloc);

	return;
}

static inline int 
in_lib(struct quiescence_timing *timing) 
{
	/* this means not inside the lib. */
	if (timing->time_out > timing->time_in) return 0;

	return 1;
}

static inline void
timing_update_remote(struct percpu_info *curr, int remote_cpu, parsec_t *parsec)
{
	struct quiescence_timing *cpu_i;
	cpu_i = &(parsec->timing_info[remote_cpu].timing);

	curr->timing_others[remote_cpu].time_in  = cpu_i->time_in;
	curr->timing_others[remote_cpu].time_out = cpu_i->time_out;

	/* We are reading remote cachelines possibly, so this time
	 * stamp reading cost is fine. */
	curr->timing_others[remote_cpu].time_updated = get_time();

	/* If remote core has information that can help, use it. */
	if (curr->timing.last_known_quiescence < cpu_i->last_known_quiescence)
		curr->timing.last_known_quiescence = cpu_i->last_known_quiescence;

	cos_mem_fence();

	return;
}

int 
parsec_sync_quiescence(quie_time_t orig_timestamp, const int waiting, parsec_t *parsec)
{
	int inlib_curr, quie_cpu, curr_cpu, first_try, i, done_i;
	quie_time_t min_known_quie;
	quie_time_t in, out, update;
	struct percpu_info *cpuinfo;
	struct quiescence_timing *timing_local;
	quie_time_t time_check;

	time_check = orig_timestamp;

	curr_cpu  = get_cpu();

	cpuinfo = &(parsec->timing_info[curr_cpu]);
	timing_local = &cpuinfo->timing;

	inlib_curr = in_lib(timing_local);
	/* ensure quiescence on the current core: either time_in >
	 * time_check, or we are not in the lib right now. */
	if (unlikely((time_check > timing_local->time_in) 
		     && inlib_curr)) {
		/* printf(">>>>>>>>>> QUIESCENCE wait error %llu %llu!\n",  */
		/*        time_check, timing_local->time_in); */
		return -EQUIESCENCE;
	}

	min_known_quie = (unsigned long long)(-1);
	for (i = 1; i < NUM_CPU; i++) {
		quie_cpu = (curr_cpu + i) % NUM_CPU;
		assert(quie_cpu != curr_cpu);

		first_try = 1;
		done_i = 0;
	re_check:
		if (time_check < timing_local->last_known_quiescence) 
			break;

		in     = cpuinfo->timing_others[quie_cpu].time_in;
		out    = cpuinfo->timing_others[quie_cpu].time_out;
		update = cpuinfo->timing_others[quie_cpu].time_updated;

		if ((time_check < in) || 
		    ((time_check < update) && (in < out))) {
			done_i = 1;
		}
		
		if (done_i) {
			/* assertion: update >= in */
			if (in < out) {
				if (min_known_quie > update) min_known_quie = update;
			} else {
				if (min_known_quie > in) min_known_quie = in;
			}
			continue;
		}

		/* If no waiting allowed, then read at most one remote
		 * cacheline per core. */
		if (first_try) {
			first_try = 0;
		} else {
			if (!waiting) return -1;
		}

		timing_update_remote(cpuinfo, quie_cpu, parsec);

		goto re_check;
	}

	/* This would be on the fast path of the single core/thd
	   case. Optimize a little bit. */
#if NUM_CPU > 1
	if (i == NUM_CPU) {
		if (inlib_curr && (min_known_quie > timing_local->time_in))
			min_known_quie = timing_local->time_in;

		assert(min_known_quie < (unsigned long long)(-1));
		/* This implies we went through all cores. Thus the
		 * min_known_quie can be used to determine global quiescence. */
		if (timing_local->last_known_quiescence < min_known_quie)
			timing_local->last_known_quiescence = min_known_quie;
		cos_mem_fence();
	}
#endif

	return 0;
}

/* force waiting for quiescence */
int 
parsec_quiescence_wait(quie_time_t orig_timestamp, parsec_t *p)
{
	/* waiting for quiescence if needed. */
	return parsec_sync_quiescence(orig_timestamp, 1, p);
}

int 
parsec_quiescence_check(quie_time_t orig_timestamp, parsec_t *p)
{
	/* non-waiting */
	return parsec_sync_quiescence(orig_timestamp, 0, p);
}

static inline struct quie_mem_meta *
quie_queue_alloc(struct quie_queue *queue, struct parsec_allocator *alloc, const int waiting)
{
	struct quie_mem_meta *ret, *head;
	int (*quie_fn)(quie_time_t, int);

	quie_fn = (int (*)(quie_time_t, int))alloc->quiesce;

	head = queue->head;
	if (!head) return NULL;

	if (quie_fn(head->time_deact, waiting)) {
		return NULL;
	}
	ret = quie_queue_remove(queue);

	assert(ret == head);

	return ret;
}

void *
parsec_alloc(size_t size, struct parsec_allocator *alloc, const int waiting)
{
	/* try free-list first */
	struct quie_queue *queue;
	struct quie_mem_meta *meta = NULL;
	struct quie_queue_slab *qwq;
	struct glb_freelist_slab *glb_freelist;
	int cpu, slab_id;

	cpu = get_cpu();
	slab_id = size2slab(size);
	assert(slab_id < QUIE_QUEUE_N_SLAB);

	qwq = alloc->qwq;
	glb_freelist = &(alloc->glb_freelist);

	queue = &(qwq[cpu].slab_queue[slab_id]);
//	printf("cpu %d, queue %p\n", cpu, queue);

	if (queue->n_items < queue->qwq_min_limit) {
		/* This will add items (new or from global freelist)
		 * onto quie_queue if possible. */
		quie_queue_fill(queue, size, alloc);
	}

	if (queue->n_items >= queue->qwq_min_limit) {
		/* Ensure the minimal amount of items on the
		 * quiescence queue. */
		meta = quie_queue_alloc(queue, alloc, waiting);
	}

	if (!meta) {
		/* printf("No memory allocated\n"); */
		return NULL;
	}

	meta->flags &= ~PARSEC_FLAG_DEACT;
	assert(meta->size >= size);

	return (char *)meta + sizeof(struct quie_mem_meta);
}

/* TODO: fix the size to make sense. */
void *
parsec_desc_alloc(size_t size, struct parsec_allocator *alloc, const int waiting)
{
	/* try free-list first */
	struct quie_queue *queue;
	struct quie_mem_meta *meta = NULL;
	struct quie_queue_slab *qwq;
	struct glb_freelist_slab *glb_freelist;
	int cpu, slab_id;

	cpu = get_cpu();
	slab_id = size2slab(size);
	assert(slab_id < QUIE_QUEUE_N_SLAB);

	qwq = alloc->qwq;
	glb_freelist = &(alloc->glb_freelist);

	queue = &(qwq[cpu].slab_queue[slab_id]);

	if (queue->n_items < queue->qwq_min_limit) {
		/* This will add items (new or from global freelist)
		 * onto quie_queue if possible. */
		quie_queue_fill(queue, size, alloc);
	}

	if (queue->n_items >= queue->qwq_min_limit) {
		/* Ensure the minimal amount of items on the
		 * quiescence queue. */
		meta = quie_queue_alloc(queue, alloc, waiting);
	}

	if (!meta) {
		/* printf("No memory allocated\n"); */
		return NULL;
	}

	meta->flags &= ~PARSEC_FLAG_DEACT;
	assert(meta->size >= size);

	return (char *)meta + sizeof(struct quie_mem_meta);
}

void 
parsec_read_lock(parsec_t *parsec) 
{
	int curr_cpu;
	quie_time_t curr_time;
	struct quiescence_timing *timing;
	
	curr_cpu  = get_cpu();
	curr_time = get_time();

	timing = &(parsec->timing_info[curr_cpu].timing);
	timing->time_in = curr_time;
	
	/* Following is needed when we have coarse granularity
	 * time-stamps (i.e. non-cycle granularity, which means we
	 * could have same time-stamp for different events). */
	timing->time_out = curr_time - 1;

	cos_mem_fence();
	
	return;
}

void 
parsec_read_unlock(parsec_t *parsec) 
{
	int curr_cpu;
	struct quiescence_timing *timing;
	
	curr_cpu = get_cpu();
	timing = &(parsec->timing_info[curr_cpu].timing);

	/* barrier, then write time stamp. */

	/* Here we don't require a full memory barrier on x86 -- only
	 * a compiler barrier is enough. */
	cmm_barrier();

	timing->time_out = timing->time_in + 1;
	
	return;
}

void 
lib_enter(parsec_t *parsec) 
{
	parsec_read_lock(parsec);
}

void 
lib_exit(parsec_t *parsec) 
{
	parsec_read_unlock(parsec);
}

/* try returning from local qwq to glb freelist. */
static inline int 
quie_queue_balance(struct quie_queue *queue, struct parsec_allocator *alloc)
{
	unsigned long thres, qwq_min;
	struct quie_mem_meta *head, *last = NULL;
	struct freelist *freelist;
	struct glb_freelist_slab *glb_freelist;
	int (*quie_fn)(quie_time_t, int);
	size_t size;
	int return_n = 0;

	quie_fn = (int (*)(quie_time_t, int))alloc->quiesce;
	glb_freelist = &alloc->glb_freelist;

	head = queue->head;
	size = head->size;
	/* assert(size == round2next_pow2(head->size)); */

	qwq_min = queue->qwq_min_limit;

	thres = qwq_min * 2;
	if (thres > alloc->qwq_max_limit)
		thres = qwq_min + (alloc->qwq_max_limit - qwq_min) / 2;

	while (queue->head && (queue->n_items > thres)) {
		/* only return quiesced items. */
		if (quie_fn(queue->head->time_deact, 0) == 0) {
			return_n++;
			last = queue->head;
			assert(last->size == size);
			quie_queue_remove(queue);
			/* construct a local list */
			last->next = queue->head;
		} else {
			break;
		}
	}

	if (return_n) {
		assert(head && last);
		/* last is the last item on the local list. */
		freelist = &(glb_freelist->slab_freelists[size2slab(size)]);

		/* add the freed item list to the global freelist. */
		ck_spinlock_lock(&(freelist->l));
		last->next = freelist->head;
		freelist->head = head;
		freelist->n_items += return_n;
		ck_spinlock_unlock(&(freelist->l));
	}

	return 0;
}

/* Add *new* items to the global freelist. no quiescence checked. */
int 
glb_freelist_add(void *item, struct parsec_allocator *alloc)
{
	struct freelist *freelist;
	struct quie_mem_meta *meta;
	
	meta = item - sizeof(struct quie_mem_meta);

	/* Should have been marked already. Set it anyway. */
	meta->flags |= PARSEC_FLAG_DEACT;

	freelist = &(alloc->glb_freelist.slab_freelists[size2slab(meta->size)]);

	ck_spinlock_lock(&(freelist->l));
	meta->next = freelist->head;
	freelist->head = meta;
	freelist->n_items++;
	ck_spinlock_unlock(&(freelist->l));

	return 0;
}

int
parsec_free(void *node, struct parsec_allocator *alloc)
{
	struct quie_mem_meta *meta;
	struct quie_queue *queue;
	int cpu, old, new;

	if (unlikely(!node || !alloc)) return -EINVAL;
	/* if ((unsigned long)node % CACHE_LINE) return -EINVAL; */

	cpu = get_cpu();
	meta = (struct quie_mem_meta *)((unsigned long)node - sizeof(struct quie_mem_meta));
	meta->time_deact = get_time();

	old = meta->flags;
	if (old & PARSEC_FLAG_DEACT) return -EINVAL;
	new = meta->flags | PARSEC_FLAG_DEACT;

	if (cos_cas((unsigned long *)(&meta->flags), old, new) != CAS_SUCCESS) return -ECASFAIL;

	queue = &(alloc->qwq[cpu].slab_queue[size2slab(meta->size)]);
	quie_queue_add(queue, meta);
	
	if (queue->n_items >= alloc->qwq_max_limit) {
		quie_queue_balance(queue, alloc);
	}
	
	return 0;
}

void *
lib_exec(void *(*func)(void *), void *arg, parsec_t *p) 
{
	void *ret;

	lib_enter(p);
	ret = func(arg);
	lib_exit(p);

	return ret;
}

/* not used for now */
static void parsec_quiesce(parsec_t *p)
{
	quie_time_t t = get_time();
	
	lib_enter(p);
	parsec_quiescence_wait(t, p);
	lib_exit(p);
}

void *
timer_update(void *arg)
{
	(void)arg;
	glb_ts.ts = 10;

	/* printf("timer thread running on cpu %d\n", (int)arg); */
	while (1) {
		spin_delay(TS_GRANULARITY);
		global_timestamp_inc();
	}
}

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
