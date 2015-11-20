/* PARSEC library implementation */
#ifndef PARSEC_HEADER_H
#define PARSEC_HEADER_H

#include <ck_queue.h>
#include <ck_spinlock.h>

//#define IN_LINUX

#ifdef IN_LINUX
//Linux headers
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sched.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#else
// Composite header files
#include <cos_config.h>
#include <cos_debug.h>
//#include <print.h>
#define printf printc
#endif

#ifndef ACCESS_ONCE
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif

#ifndef cmm_barrier
#define cmm_barrier() __asm__ __volatile__ ("" : : : "memory")
#endif

#ifndef cos_throw
#define cos_throw(label, errno) { ret = (errno); goto label; }
#endif

typedef unsigned long long quie_time_t;

// for 99 percentile w/ limited memory
//#define THRES (3000)

/* used in Linux tests only */
#define MEM_SIZE (128*1024*1024)

//#define QUIE_QUEUE_LIMIT (1024*8)

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

enum {
	PARSEC_FLAG_DEACT = 1,
};

/* void * worker(void *arg); */

static inline void
spin_delay(unsigned long long cycles)
{
	unsigned long long s, e, curr;

	rdtscll(s);
	e = s + cycles;

	curr = s;
	while (curr < e) rdtscll(curr);

	return;
}

void set_prio(void);

void meas_sync_start(void);
void meas_sync_end(void);

#define PACKED __attribute__((packed))

#ifdef IN_LINUX
extern int cpu_assign[41];

//#define CACHE_LINE (64)
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE)))
#define PAGE_SIZE (4096)
#define PAGE_ALIGNED  __attribute__((aligned(PAGE_SIZE)))
#define EQUIESCENCE (200)

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
cos_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
	char z;
	__asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (old)
			     : "memory", "cc");
	return (int)z;
}

static inline void cos_mem_fence(void)
{
        __asm__ __volatile__("mfence" ::: "memory");
}

void thd_set_affinity(pthread_t tid, int cpuid);
static inline pthread_t
create_timer_thd(int cpuid)
{
	pthread_t t;
	int ret;

	ret = pthread_create(&t, 0, timer_update, (void *)cpuid);
	if (ret < 0) exit(-1);
	assert(t);

	thd_set_affinity(t, cpuid);

	return t;
}
#endif

/********************************************************/

#ifndef likely
#define unlikely(x)   __builtin_expect(!!(x), 0)
#define likely(x)     __builtin_expect(!!(x), 1)
#endif

/* 2 cache lines for this struct to avoid false sharing caused by
 * prefetching. */
struct quiescence_timing {
	volatile quie_time_t time_in, time_out;
	volatile quie_time_t last_known_quiescence;
	char __padding[CACHE_LINE*2 - 3*sizeof(quie_time_t)];
} CACHE_ALIGNED PACKED;

struct other_cpu {
	quie_time_t time_in, time_out;
	quie_time_t time_updated;
};

struct percpu_info {
	/* Quiescence_timing info of this CPU */
	struct quiescence_timing timing;
	/* Quiescence_timing info of other CPUs known by this CPU */
	struct other_cpu timing_others[NUM_CPU];
	/* padding an additional cacheline for prefetching */
	char __padding[CACHE_LINE*2 - (((sizeof(struct other_cpu)*NUM_CPU)+sizeof(struct quiescence_timing)) % CACHE_LINE)];
} CACHE_ALIGNED PACKED;

struct global_timestamp {
	quie_time_t ts;
	char __padding[CACHE_LINE*2 - sizeof(quie_time_t)];
} PACKED CACHE_ALIGNED;

volatile struct global_timestamp glb_ts CACHE_ALIGNED;

/* Instead of relying on synced rdtsc, this approach emulates a global
 * timestamp at coarse granularity. Only a single thread should be
 * calling the increment function. */

static inline void
global_timestamp_inc(void)
{
	ck_pr_faa_uint((unsigned int *)&(glb_ts.ts), 2);
	cos_mem_fence();
}

static inline quie_time_t
get_global_timestamp(void)
{
	return glb_ts.ts;
}

#ifdef IN_LINUX
extern __thread int thd_local_id;

/* For now, we assume per-thread instead of per-cpu. */
static inline int
get_cpu(void)
{
	return thd_local_id;
}
#else

static inline int
get_cpu(void)
{
	return cos_cpuid();
}

#endif

#define SYNC_USE_RDTSC

static inline quie_time_t
get_time(void)
{
	quie_time_t curr_t;

#ifdef SYNC_USE_RDTSC
	rdtscll(curr_t);
#else
	curr_t = get_global_timestamp();
#endif
	return curr_t;
}

#define TS_GRANULARITY (2000)

/********************************************************/

struct quie_mem_meta {
	quie_time_t time_deact; /* used for tracking quiescence */
	size_t size;
	int flags;
	struct quie_mem_meta *next; /* linked-list (freelist or quiescence_waiting queue) */
//	void *mem;  /* pointer to the memory region (i.e. next cacheline) */
	/* char __padding[CACHE_LINE - sizeof(quie_time_t) - sizeof(void *)  */
	/* 	       - sizeof(struct quie_mem_meta *)- sizeof(size_t)]; */
} ;
typedef struct quie_mem_meta quie_meta_t;
static inline unsigned int
round2next_pow2(unsigned int v)
{
	/* compute the next highest power of 2 of 32-bit v */
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}

struct quie_queue {
	struct quie_mem_meta *tail; /* adding to here when freeing */
	struct quie_mem_meta *head; /* removing from here when allocating */
	size_t n_items;
	unsigned int qwq_min_limit;
} PACKED;


/* FIXME: for namespace allocator, may not need the slab allocation. */

/* Per cpu / thread */
#define QUIE_QUEUE_N_SLAB (23)
struct quie_queue_slab {
	struct quie_queue slab_queue[QUIE_QUEUE_N_SLAB];
	/* Padding to avoid false sharing. 2 cachelines for prefetching. */
	char __padding[CACHE_LINE*2 - ((sizeof(struct quie_queue)*QUIE_QUEUE_N_SLAB) % CACHE_LINE)];
};

/* The global freelist used for balancing. */
struct freelist {
	struct quie_mem_meta *head;
	ck_spinlock_t l;
	size_t n_items;
	/* Padding to avoid false sharing. 2 cachelines for prefetching. */
	char __padding[CACHE_LINE*2 - sizeof(struct quie_mem_meta *)
		       - sizeof(ck_spinlock_t) - sizeof(size_t)];
} PACKED;

struct glb_freelist_slab {
	struct freelist slab_freelists[QUIE_QUEUE_N_SLAB];
};

struct parsec_allocator {
	/* global freelist used for balancing*/
	struct glb_freelist_slab glb_freelist;
	/* local (per thd or cpu) quiescence waiting queue (qwq) */
	/* For efficiency, I don't allocate the slab queues
	 * dynamically. Instead just integrate them in the same
	 * data-structure here. */
	struct quie_queue_slab qwq[NUM_CPU];
	/* struct quie_queue_slab *qwq; */

	/* # of private queues, usually 1 per cpu. for boundary
             check. */
	unsigned int n_local;
	/* Only allocate from the qwq when we have at least this many items / capacity */
	unsigned int qwq_min_limit;
	/* threshold of when to return to global freelist */
	unsigned int qwq_max_limit;

	/* Quiescence function. Could be different from the default,
	 * which provides library quiescence. */
	void *quiesce;
};

struct parsec {
	struct percpu_info timing_info[NUM_CPU] CACHE_ALIGNED;
	struct parsec_allocator mem;
} CACHE_ALIGNED;
typedef struct parsec parsec_t ;

struct parsec_ns {
	/* resource table of the namespace */
	volatile void *tbl;
	unsigned long item_sz;

	/* function pointers next */
	void *lookup;
	void *alloc;
	void *free;

	void *init;
	void *expand;

	/* The parallel section that protects this name space. It
	 * contains the timing info. */
	parsec_t *parsec;

	/* thread / cpu mapping to private allocation queues */
	void *ns_mapping;
	/* The allocator that includes the global freelist and local
	 * quiescence-waiting queue. */
	struct parsec_allocator allocator;
};
typedef struct parsec_ns parsec_ns_t ;

int parsec_sync_quiescence(quie_time_t orig_timestamp, const int waiting, parsec_t *p);

//void *q_alloc(size_t size, const int waiting);
//void *q_alloc(size_t size, const int waiting, struct quie_queue_slab *qwq, struct glb_freelist_slab *glb_freelist);
void *parsec_alloc(size_t size, struct parsec_allocator *alloc, const int waiting);
void *parsec_desc_alloc(size_t size, struct parsec_allocator *alloc, const int waiting);

int parsec_free(void *node, struct parsec_allocator *alloc);

void parsec_read_lock(parsec_t *parsec);
void parsec_read_unlock(parsec_t *parsec);

void *lib_exec(void *(*func)(void *), void *arg, parsec_t *p);

void parsec_init(parsec_t *p);
void q_debug(void);

void * timer_update(void *arg);
void * worker(void *arg);

static inline int
parsec_item_active(void *item)
{
	struct quie_mem_meta *i = item - sizeof(struct quie_mem_meta);
	return !(i->flags & PARSEC_FLAG_DEACT);
}

static inline int
parsec_item_size(void *item)
{
	struct quie_mem_meta *i = item - sizeof(struct quie_mem_meta);
	return i->size;
}

static inline int
parsec_desc_activate(void *item)
{
	struct quie_mem_meta *m = item - sizeof(struct quie_mem_meta);
	/* no contention should happen for new items -- they are not
	 * activated yet. */
	m->flags &= ~PARSEC_FLAG_DEACT;
	return 0;
}

static inline int
parsec_desc_deact(void *item)
{
	struct quie_mem_meta *m = item - sizeof(struct quie_mem_meta);
	unsigned long old, new;

	old = m->flags;
	if (old & PARSEC_FLAG_DEACT) return -EINVAL;

	new = old | PARSEC_FLAG_DEACT;
	/* detect contention */
	if (cos_cas((unsigned long *)&m->flags, old, new) != CAS_SUCCESS) return -ECASFAIL;

	return 0;
}

/* used to add free item to the head */
static inline void
quie_queue_add_head(struct quie_queue *queue, struct quie_mem_meta *meta)
{
	if (queue->head) {
		assert(queue->tail && queue->tail->next == NULL);
		meta->next = queue->head;
	} else {
		assert(queue->n_items == 0);
		/* empty queue */
		queue->tail = meta;
		meta->next  = NULL;
	}

	queue->head = meta;
	queue->n_items++;
	/* printc("queue add head: %d items\n", queue->n_items); */

	return;
}

static inline int size2slab(size_t orig_size);

static inline int
qwq_add_freeitem(void *item, struct parsec_allocator *alloc)
{
	struct quie_queue *queue;
	struct quie_mem_meta *meta;

	meta = item - sizeof(struct quie_mem_meta);
	/* Should have been marked already. Set it anyway. */
	meta->flags |= PARSEC_FLAG_DEACT;

	queue = &(alloc->qwq[cos_cpuid()].slab_queue[size2slab(meta->size)]);
	quie_queue_add_head(queue, meta);

	return 0;
}

static inline void
quie_queue_add(struct quie_queue *queue, struct quie_mem_meta *meta)
{
	/* per thread for now. */
	meta->next = NULL;

	if (queue->tail) {
		assert(queue->tail->next == NULL);
		queue->tail->next = meta;
	} else {
		assert(queue->n_items == 0);
		/* empty queue */
		queue->head = meta;
	}

	queue->tail = meta;
	queue->n_items++;

	return;
}

static inline struct quie_mem_meta *
quie_queue_remove(struct quie_queue *queue)
{
	struct quie_mem_meta *ret;

	/* per thread for now. */
	ret = queue->head;
	if (!ret) return NULL;

	queue->head = ret->next;
	ret->next = NULL;
	queue->n_items--;

	if (queue->n_items == 0) queue->tail = NULL;

	/* printc("queue remove: %d items\n", queue->n_items); */

	return ret;
}

static inline int
size2slab(size_t orig_size)
{
	unsigned int i;
	unsigned int size = orig_size;

	if (!size) return 0;

	/* less than a cacheline uses same slab. */
	size = round2next_pow2(orig_size) / CACHE_LINE;

	if (orig_size < CACHE_LINE) size = 1;

	/* TODO: use the instruction to get # of trailing zeros */
	for (i = 0; i < QUIE_QUEUE_N_SLAB; i++) {
		size >>= 1;
		if (!size) break;
	}

	if (unlikely(size)) {
		printc("ERROR: no slab for size %u\n", orig_size);
		return -1;
	}

	return i;
}

/* return free mem to global when balance limit */
//#define QUIE_QUEUE_BALANCE_UPPER_LIMIT (QUIE_QUEUE_LIMIT * 4)
//#define QUIE_QUEUE_BALANCE_LOWER_LIMIT (QUIE_QUEUE_LIMIT * 2)

int glb_freelist_add(void *item, struct parsec_allocator *alloc);

#define STRIDE 16
static inline int
glb_freelist_get(struct quie_queue *queue, size_t size, struct parsec_allocator *alloc)
{
	struct quie_mem_meta *next, *head, *last;
	struct freelist *freelist;
	struct glb_freelist_slab *glb_freelist;
	int i, n_alloc, needed;
	unsigned long thres;
	unsigned long qwq_min;

	if (!size) return 0;

	glb_freelist = &alloc->glb_freelist;
	freelist = &glb_freelist->slab_freelists[size2slab(size)];

	ck_spinlock_lock(&freelist->l);
	head = last = next = freelist->head;

	qwq_min = queue->qwq_min_limit;

	/* Policy. Should be set separately. */
	thres = qwq_min * 2;
	if (thres > alloc->qwq_max_limit)
		thres = qwq_min + (alloc->qwq_max_limit - qwq_min) / 2;

	needed = thres - queue->n_items;
	if (qwq_min > STRIDE)
		if (needed % STRIDE) needed += (STRIDE - needed % STRIDE);

	for (i = 0; (i < needed) && next; i++) {
		last = next;
		next = last->next;
	}
	n_alloc = i;

	assert(freelist->n_items >= (unsigned int)n_alloc);
	freelist->n_items -= n_alloc;
	freelist->head = next;

	ck_spinlock_unlock(&freelist->l);

	if (n_alloc > 0) {
		/* we get from head to last, adding to the local queue */
		/* they are free items -- so add them to the head. */
		last->next = queue->head;
		queue->head = head;
		queue->n_items += n_alloc;

		if (queue->tail == NULL) queue->tail = last;
	}

	/* printf("size %d: local %d items (got %d, needed %d), glb %d now\n", */
	/*        size, queue->n_items, n_alloc, needed, freelist->n_items); */

	return n_alloc;
}

/* PARSEC_HEADER_H */
#endif
