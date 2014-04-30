#include <omp.h>
#include <stdio.h>

#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>

//#define DISABLE

/* //cos specific  */
#include <cos_alloc.h>
#include <cos_synchronization.h>
#include <parlib.h>
#include <mem_mgr_large.h>
#include <acap_pong.h>
#include <heap.h>

#define ITER (1*1000*1000)

//#define CACHELINE_MEAS
#define DS_MEAS

#define GAP_US (3)
/////////////////////////////////////////////////////////////////////////////
//#define ENABLE_TDMA
#define TDMA_SLOT             ((int)(5 * 1000 * CPU_GHZ))
#define TDMA_CORES_PER_SLOT   (1)

#define TDMA_TAIL             ((int)(2.5 * 1000 * CPU_GHZ))
#define TDMA_DRIFT            ((int)(0 * 1000 * CPU_GHZ))

//>>>>>>>>>>>>>>>>>>>>>>>>>>>
#if TDMA_CORES_PER_SLOT <= NUM_CORE_PER_SOCKET
#define TDMA_SOCKET_NSLOT     (NUM_CORE_PER_SOCKET / TDMA_CORES_PER_SLOT) // how many slots each socket
#define TDMA_NUM_SLOTS        ((int)(NUM_CPU_SOCKETS * TDMA_SOCKET_NSLOT))
#else
#define TDMA_NUM_SLOTS        ((int)(NUM_CPU / TDMA_CORES_PER_SLOT))
#endif
#define TDMA_WINDOW           ((int)(TDMA_SLOT * TDMA_NUM_SLOTS))
////////////////////////////////////////////////////////////////////////////
#define ENABLE_RATE_LIMIT

#ifndef ENABLE_TDMA
#define READER_CORE (-1)
#else
// TDMA all cores writing
#define READER_CORE (-1)
#endif
///////////////////////////////////////////////////////////////////////////
#define printf printc

unsigned long long tsc_start(void)
{
	unsigned long cycles_high, cycles_low; 
	asm volatile ("movl $0, %%eax\n\t"
		      "CPUID\n\t"
		      "RDTSC\n\t"
		      "movl %%edx, %0\n\t"
		      "movl %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low) :: 
		      "%eax", "%ebx", "%ecx", "%edx");

	return ((unsigned long long)cycles_high << 32) | cycles_low;
}

unsigned long long tsc_end(void)
{
	/* This RDTSCP doesn't prevent memory re-ordering!. */
	unsigned long cycles_high1, cycles_low1; 
	asm volatile("RDTSCP\n\t"
		     "movl %%edx, %0\n\t"
		     "movl %%eax, %1\n\t"
		     "movl $0, %%eax\n\t"
		     "CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: 
		     "%eax", "%ebx", "%ecx", "%edx");

	return ((unsigned long long)cycles_high1 << 32) | cycles_low1;
}

unsigned long long rdtsc(void)
{
	unsigned long cycles_high, cycles_low; 

	asm volatile ("RDTSCP\n\t" 
		      "movl %%edx, %0\n\t" 
		      "movl %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low) : : "%eax", "%edx"); 

	return ((unsigned long long)cycles_high << 32 | cycles_low);
}

int *detector = (int *)0x4c3f0000;

#define CACHELINE_SIZE 64

int delay(int us) {
	unsigned long long s,e;
	volatile int mem = 0;

	s = rdtsc();
	while (1) {
		e = rdtsc();
		if (e - s > CPU_GHZ*1000*us) return 0; // x us
		mem++;
	}

	return 0;
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

unsigned int reader_view[NUM_CPU_COS];

int cpu_assign[40] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		      1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
		      2, 6, 10, 14, 18, 22, 26, 30, 34, 38,
		      3, 7, 11, 15, 19, 23, 27, 31, 35, -1};

//int cpu_assign[40] = {0, 1, -1};
volatile int n_cores;
volatile int rate_gap;

void meas_sync_start(void) {
	int cpu = cos_cpuid();
	ck_pr_store_int(&thd_active[cpu].done, 0);
	ck_pr_store_int(&thd_active[cpu].avg, 0);
	ck_pr_store_int(&thd_active[cpu].max, 0);
	ck_pr_store_int(&thd_active[cpu].read_avg, 0);
	ck_pr_store_int(&thd_active[cpu].read_max, 0);

	if (cpu == 0) {
		int k = 1;
		while (k < n_cores) {
			while (1) {
				if (ck_pr_load_int(&thd_active[cpu_assign[k]].accessed)) break;
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
	int cpu = cos_cpuid();
	ck_pr_store_int(&thd_active[cpu].accessed, 0);

	if (cpu == 0) { // output!!!
//		printf("test done %d, syncing\n", NUM_CPU_COS);
		// sync first!
		for (i = 1; i < n_cores;i++) {
			while (1) {
				if (ck_pr_load_int(&thd_active[cpu_assign[i]].done)) break;
			}
		}
		ck_pr_store_int(&thd_active[0].done, 1);
	} else {
		ck_pr_store_int(&thd_active[cpu].done, 1);

		while (ck_pr_load_int(&thd_active[0].done) == 0) ;
	}
}

__attribute__((packed)) struct shared_cacheline {
	int mem;
	char _pad[CACHELINE_SIZE - sizeof(int)];
} CACHE_ALIGNED;

struct shared_cacheline shared_mem;
struct shared_cacheline interfere_mem;

static inline int null_op(int cpu, unsigned long long tsc) {
	return 0;
}

//#define MEM_FENCE ck_pr_fence_strict_memory()
#define MEM_FENCE

static inline int meas_faa_reader(int cpu, unsigned long long tsc) {
	volatile unsigned int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);
	MEM_FENCE;
	assert(stk_mem != tsc);

	// op done!
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_faa(int cpu, unsigned long long tsc) {
	volatile unsigned int stk_mem;

	stk_mem = ck_pr_faa_int(&shared_mem.mem, 1);
	MEM_FENCE;

	// op done!
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_cas_reader(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);
	MEM_FENCE;

	// op done!
	assert(stk_mem < NUM_CPU_COS);
//	reader_view[stk_mem]++;
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_cas(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = shared_mem.mem;
	ck_pr_cas_int(&shared_mem.mem, (int)stk_mem, (int) cpu);
	MEM_FENCE;

	// op done!
	assert(stk_mem < NUM_CPU_COS);
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_store_reader(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);
	MEM_FENCE;

	// op done!
	assert(stk_mem < NUM_CPU_COS);
//	reader_view[stk_mem]++;
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_store(int cpu, unsigned long long tsc) {
	volatile int stk_mem = 0;

	ck_pr_store_int(&shared_mem.mem, cpu);
	MEM_FENCE;

	// op done!
	assert(stk_mem < NUM_CPU_COS);
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_reader(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);
	MEM_FENCE;

	// op done!
//	assert((unsigned int)stk_mem != tsc);
//	reader_view[stk_mem]++;
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_interference(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = ck_pr_faa_int(&interfere_mem.mem, 1);
	MEM_FENCE;

	return 0;
}

/*************************************************************/
// higher level data structures next.
////////////////////////////////////////////////

///////////////spin lock and ticket lock!
#include <ck_spinlock.h>
ck_spinlock_cas_t spinlock = CK_SPINLOCK_CAS_INITIALIZER;
static inline int meas_spinlock(int cpu, unsigned long long tsc) {
	ck_spinlock_cas_lock(&spinlock);
	ck_spinlock_cas_unlock(&spinlock);
	// lock take then release
	return 0;
}

static inline int meas_spinlock_eb(int cpu, unsigned long long tsc) {
	ck_spinlock_cas_lock_eb(&spinlock);
	ck_spinlock_cas_unlock(&spinlock);
	// lock take then release
	return 0;
}

ck_spinlock_ticket_t ticketlock = CK_SPINLOCK_TICKET_INITIALIZER;
static inline int meas_ticketlock(int cpu, unsigned long long tsc) {
	// lock take then release
	ck_spinlock_ticket_lock(&ticketlock);
	ck_spinlock_ticket_unlock(&ticketlock);
	return 0;
}

static inline int meas_ticketlock_eb(int cpu, unsigned long long tsc) {
	// lock take then release
	ck_spinlock_ticket_lock_pb(&ticketlock, 0);
	ck_spinlock_ticket_unlock(&ticketlock);
	return 0;
}

static ck_spinlock_mcs_t CK_CC_CACHELINE mcs_lock = CK_SPINLOCK_MCS_INITIALIZER;
static inline int meas_mcslock(int cpu, unsigned long long tsc) {
        ck_spinlock_mcs_context_t node CACHE_ALIGNED;

        ck_spinlock_mcs_lock(&mcs_lock, &node);
        ck_spinlock_mcs_unlock(&mcs_lock, &node);

        return 0;
}

/////////////////heap!
struct hentry {
	int index, value;
};

int c(void *a, void *b) { return ((struct hentry*)a)->value >= ((struct hentry*)b)->value; }
void u(void *e, int pos) { ((struct hentry*)e)->index = pos; }

static struct hentry es[NUM_CPU_COS];
struct heap *h;

static inline int meas_heap_mcs(int cpu, unsigned long long tsc) {
        ck_spinlock_mcs_context_t node CACHE_ALIGNED;

        ck_spinlock_mcs_lock(&mcs_lock, &node);
	es[cpu].value = (int)tsc;
	heap_add(h, &es[cpu]);
        ck_spinlock_mcs_unlock(&mcs_lock, &node);

        ck_spinlock_mcs_lock(&mcs_lock, &node);
	heap_remove(h, es[cpu].index);
        ck_spinlock_mcs_unlock(&mcs_lock, &node);

        return 0;
}

static inline int meas_heap_ticket(int cpu, unsigned long long tsc) {
	ck_spinlock_ticket_lock(&ticketlock);
	es[cpu].value = (int)tsc;
	heap_add(h, &es[cpu]);
	ck_spinlock_ticket_unlock(&ticketlock);

	ck_spinlock_ticket_lock(&ticketlock);
	heap_remove(h, es[cpu].index);
	ck_spinlock_ticket_unlock(&ticketlock);

        return 0;
}

///////////////list!
#include <ck_queue.h>
struct test {
	int value;
	CK_LIST_ENTRY(test) list_entry;
	char __pad[CACHE_LINE - sizeof(int) - sizeof(CK_LIST_ENTRY(test))];
} CACHE_ALIGNED;
static CK_LIST_HEAD(test_list, test) head = CK_LIST_HEAD_INITIALIZER(head);

struct test list_items[NUM_CPU] CACHE_ALIGNED;

static inline int meas_list(int cpu, unsigned long long tsc) {
	struct test *a = &list_items[cpu];
//	a->value = cpu;
	//insert!
	ck_spinlock_ticket_lock(&ticketlock);
	CK_LIST_INSERT_HEAD(&head, a, list_entry);
	ck_spinlock_ticket_unlock(&ticketlock);
	//and remove.
	ck_spinlock_ticket_lock(&ticketlock);
	CK_LIST_REMOVE(a, list_entry);
	ck_spinlock_ticket_unlock(&ticketlock);
	return 0;
}

static inline int meas_list_mcs(int cpu, unsigned long long tsc) {
        ck_spinlock_mcs_context_t node CACHE_ALIGNED;

	struct test *a = &list_items[cpu];
//	a->value = cpu;
	//insert!
        ck_spinlock_mcs_lock(&mcs_lock, &node);
	CK_LIST_INSERT_HEAD(&head, a, list_entry);
        ck_spinlock_mcs_unlock(&mcs_lock, &node);

	//and remove.
        ck_spinlock_mcs_lock(&mcs_lock, &node);
	CK_LIST_REMOVE(a, list_entry);
        ck_spinlock_mcs_unlock(&mcs_lock, &node);

	return 0;
}

///////////////hash table!
#include <ck_ht.h>
//#include "ht.h"

/* static inline void meas_hashtable(int cpu, unsigned long long tsc) { */
/* 	table_insert(cpu); */
/* 	table_get(cpu); */
/* 	table_remove(cpu); */
/* } */

//////////////////////
#include <ck_stack.h>
struct entry {
	int value;
	ck_stack_entry_t next;
	char __pad[CACHE_LINE - sizeof(int) - sizeof(ck_stack_entry_t)];
} CACHE_ALIGNED;

static ck_stack_t stack CACHE_ALIGNED;

CK_STACK_CONTAINER(struct entry, next, getvalue)

ck_stack_entry_t stk_items[NUM_CPU] CACHE_ALIGNED;

static inline int meas_stack(int cpu, unsigned long long tsc) {
	ck_stack_entry_t *item = &stk_items[cpu];
	struct entry *entry = NULL;

	ck_stack_push_upmc(&stack, item);

	ck_stack_entry_t *ref = NULL;
	while (ck_stack_trypop_upmc(&stack, &ref) == false) ;

		/* while (ck_stack_trypop_upmc(&stack, &ref) == false) */
		/* 	ck_pr_stall(); */
		/* assert(ref); */
		/* entry = getvalue(ref); */

	assert(ref);
//	entry = getvalue(ref);

	// not supported
	/* ref = ck_stack_pop_mpmc(&stack); */
	/* assert(ref); */
	/* entry = getvalue(ref); */
	return 0;
}
/////////////////////
/////////////////////////////////////////////////////////////////

static inline int meas_op(int (*op)(int cpu, unsigned long long tsc), char *name, unsigned long long gap) {
	//every core calls here!
	volatile int timer_if;
	unsigned long long s0, e0 = 0;
	int ii, k;
	int cpu = cos_cpuid();

	unsigned int socket = (unsigned int)(cpu % NUM_CPU_SOCKETS), slot;
#ifdef ENABLE_TDMA
#if TDMA_CORES_PER_SLOT <= NUM_CORE_PER_SOCKET
	slot = (socket * TDMA_SOCKET_NSLOT) + (cpu/NUM_CPU_SOCKETS) / TDMA_CORES_PER_SLOT;
#else
	if (TDMA_NUM_SLOTS > 2) {
		printc("Wrong NUM_SLOTS!\n");
		return -1;
	}
	slot = socket / TDMA_NUM_SLOTS;
#endif
#endif
	/* printc("cpu %d: slot %d\n", cpu, slot); */
#ifndef ENABLE_RATE_LIMIT 
	assert(gap == 0);
#endif
	ck_pr_store_int(&shared_mem.mem, 0);
	meas_sync_start();
	unsigned long long sum = 0, max = 0, maxss[10], sum2 = 0, find_max = 0, read_cnt = 0;
	unsigned long long s,e;
	int if_detected = 0, ret;
	unsigned long cycles_high, cycles_low, cycles_high1, cycles_low1, cycles_high2, cycles_low2;
	unsigned long long read_sum = 0, read_max = 0;

	s0 = tsc_start();

	for (k = 0; k < 10; k++) {
		maxss[k] = 0;
	}

	int our_slot = 1;
	for (ii = 0; ii < ITER; ii++) {
		s = tsc_start();
#ifdef ENABLE_TDMA
		if (   ((s % TDMA_WINDOW) / TDMA_SLOT) == slot // our slot
			&& ((s % TDMA_SLOT) > (TDMA_DRIFT+TDMA_TAIL))      )// and not in drift
		{
			/* benchmark here! */
			op(cpu, s);
			/* benchmark done! */
			our_slot = 1;
//			printc("core %d socket %d writing\n", cpu, socket);
		} else {
			if (cpu > 20) {
				meas_reader(cpu, s); // reading the line!
			} else {
				meas_interference(cpu, s);
			}
			our_slot = 0;
		}
#else
		/* benchmark here! */
		op(cpu, s);
		/* benchmark done! */
#endif
		e = tsc_start();

		/* next the timer hack! */
		timer_if = ck_pr_load_int(&(detector[cpu*16]));
		if (unlikely(timer_if == 1)) {
			ii--;
			if_detected++;
			ck_pr_store_int(&(detector[cpu*16]), 0);
			find_max = 0;
			delay(50);
			continue;
		}

		/* no timer interference if gets here! */
		find_max = e - s;

#ifdef ENABLE_TDMA
		if (our_slot) {
#else
		if (cpu > READER_CORE) {
#endif
			assert(sum + find_max > sum);
			sum += find_max;
			if (find_max > max) max = find_max;
		} else {
			assert(read_sum + find_max > read_sum);
			read_sum += find_max;
			read_cnt++;
			if (find_max > read_max) read_max = find_max;

			/* for (k = 0; k < 10; k++) { */
			/* 	if (maxss[k] < find_max) { */
			/* 		maxss[k] = find_max; */
			/* 		break; */
			/* 	} */
			/* } */
#ifndef ENABLE_TDMA
			if (ck_pr_load_int(&thd_active[READER_CORE+1].done)) break;
#endif
			ii--;
			continue; // no reading gap.
		}

#ifdef ENABLE_RATE_LIMIT
		/* Rate limit here. */
		e0 = e;
		while ((e0 - e) < (unsigned int)gap) {
			e0 = tsc_start();
		}
#endif
	}
	e0 = tsc_start();
	sum2 = e0 - s0;

	/* for (k = 0; k < 10; k++) { */
	/* 	printc("%s cpu %d, %d: %llu\n",name, cpu, k, maxss[k]); */
	/* } */

#ifndef ENABLE_TDMA
	if (read_sum == 0 && (((sum2 - sum)/ITER - gap) > 2000) && (((sum2 - sum)/ITER - gap) > (sum / ITER * 2))) {
		printc("\n\n\n\n !!!!!!!!!!!!%s cpu %ld ------------------- per op overhead %llu\n\n\n\n\n",
		       name, cos_cpuid(), (sum2-sum)/ITER - gap);
//		BUG();
	}
#endif

	if (read_cnt) {
		printc("%s cpu %ld (ticks tot %d,%llu): write avg %llu max %llu read avg %llu max %llu read_cnt %llu\n",
		       name, cos_cpuid(), if_detected, (unsigned long long)((sum+read_sum)/(CPU_GHZ*10*1000000)), sum/ITER, max, read_sum/read_cnt, read_max, read_cnt);

		ck_pr_store_int(&(thd_active[cos_cpuid()].read_avg), (int)(read_sum/read_cnt));
		ck_pr_store_int(&(thd_active[cos_cpuid()].read_max), (int)read_max);
	} else {
		printc("%s cpu %ld (ticks tot %d,%d,%d): avg %llu max %llu\n",
		       name, cos_cpuid(), if_detected, (int)((sum2-gap*ITER)/(CPU_GHZ*10*1000000)), (int)(sum/(CPU_GHZ*10*1000000)), sum/ITER, max);
	}

	ck_pr_store_int(&(thd_active[cos_cpuid()].avg), (int)(sum/ITER));
	ck_pr_store_int(&(thd_active[cos_cpuid()].max), (int)max);
	meas_sync_end();
	if (cpu == 0) {
		int i;
		int avg, max = 0, curr_max, cnt = 0, cnt2 = 0;
		unsigned long long sum = 0, sum2 = 0, tdma_read_sum = 0;
		/* if (op == meas_cas || op == meas_store) { */
		/* 	printc("reader view...\n"); */
		/* 	for (i=0;i < NUM_CPU_COS;i++) { */
		/* 		printc("core %d: %u\n", i, reader_view[i]); */
		/* 		reader_view[i] = 0; */
		/* 	} */
		/* } */
		/* printc("%s finished!\n", name); */
		int reader_avg = 0, reader_max = 0;
		int tdma_read_avg, tdma_read_max = 0;
		
		for (i = 0; i < NUM_CPU_COS; i++) {
			if (i > READER_CORE) {
				avg = ck_pr_load_int(&(thd_active[i].avg));
				if (avg) {
					cnt++;
					sum += avg;
					curr_max = ck_pr_load_int(&(thd_active[i].max));
					if (curr_max > max) 
						max = curr_max;
					
					tdma_read_avg = ck_pr_load_int(&(thd_active[i].read_avg));
					tdma_read_sum += tdma_read_avg;
					curr_max = ck_pr_load_int(&(thd_active[i].read_max));
					if (curr_max > tdma_read_max) 
						tdma_read_max = curr_max;
				}
			} else {
				avg = ck_pr_load_int(&(thd_active[i].read_avg));
				if (avg) {
					cnt2++;
					sum2 += avg;
					curr_max = ck_pr_load_int(&(thd_active[i].read_max));
					if (curr_max > reader_max) 
						reader_max = curr_max;
				}
			}

			ck_pr_store_int(&(thd_active[i].avg), 0);
			ck_pr_store_int(&(thd_active[i].max), 0);
			ck_pr_store_int(&(thd_active[i].read_avg), 0);
			ck_pr_store_int(&(thd_active[i].read_max), 0);
		}
		if (cnt) {
			avg = sum / cnt;
			if (cnt2) reader_avg = sum2/cnt2;
#ifdef ENABLE_TDMA	
			tdma_read_avg = tdma_read_sum / cnt;
			printc(">>>>>>>>>>>>>>>>%s sum ncpu %d: avg %d max %d TDMA reads avg %d max %d\n", 
			       name, cnt+cnt2, avg, max, tdma_read_avg, tdma_read_max);
#else
			printc(">>>>>>>>>>>>>>>>%s sum ncpu %d: avg %d max %d reader avg %d max %d\n", 
			       name, cnt+cnt2, avg, max, reader_avg, reader_max);
#endif
		}		
	}

	return 0;
}


static inline int meas_ptregs(int cpu, unsigned long long tsc) 
{

/* struct pt_regs { */
/*         long bx; */
/*         long cx; */
/*         long dx; */
/*         long si; */
/*         long di; */
/*         long bp; */
/*         long ax; */
/*         long ds; */
/*         long es; */
/*         long fs; */
/*         long gs; */
/*         long orig_ax; */
/*         long ip; */
/*         long cs; */
/*         long flags; */
/*         long sp; */
/*         long ss; */
/* }; */

//	struct pt_regs regs;
	
	asm volatile ("subl $4, %%esp\n\t" // skip ss
		      "pushl %%ecx\n\t"    // user-esp
		      "subl $8, %%esp\n\t" // skip flags, cs
		      "pushl %%edx\n\t"    // user-eip
		      "subl $20, %%esp\n\t"// skip flags, cs
		      "pushl %%eax;\n\t"
		      "pushl %%ebp;\n\t"
		      "pushl %%edi;\n\t"
		      "pushl %%esi;\n\t"
		      "pushl %%edx;\n\t"
		      "pushl %%ecx;\n\t"
		      "pushl %%ebx;\n\t" // push done.
		      "popl %%ebx;\n\t"
		      "popl %%ecx;\n\t"
		      "popl %%edx;\n\t"
		      "popl %%esi;\n\t"
		      "popl %%edi;\n\t"
		      "popl %%ebp;\n\t"
		      "popl %%eax;\n\t"
		      "addl $20, %%esp\n\t"
		      "popl %%edx;\n\t"
		      "addl $8, %%esp\n\t"
		      "popl %%ecx;\n\t"
		      "addl $4, %%esp\n\t"
		      :
		);

	return 0;
}

static inline void go_par(int ncores) {
	int j;

	printc("Parallel benchmark: measuring %d cores, rate_gap %d\n", ncores, rate_gap);

/* 	if (rate_gap == 0) { */
/* #pragma omp parallel for */
/* 		for (j = 0; j < ncores; j++) */
/* 		{ */
/* 			// per core below! */
/* 			assert(j == omp_get_thread_num()); */
/* 			meas_op(null_op, "rdtsc_cost", rate_gap); */
/* 		} */
/* 	} */
#ifdef CACHELINE_MEAS
#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		assert(cos_cpuid() == cpu_assign[j]);
		// per core below!
		assert(j == omp_get_thread_num());

		if (cos_cpuid() > READER_CORE)  meas_op(meas_faa, "faa", rate_gap);
		else                            meas_op(meas_faa_reader, "faa", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		if (cos_cpuid() > READER_CORE)  meas_op(meas_store, "store", rate_gap);
		else                            meas_op(meas_store_reader, "store", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		if (cos_cpuid() > READER_CORE)  meas_op(meas_cas, "cas", rate_gap);
		else                            meas_op(meas_cas_reader, "cas", rate_gap);
	}
#endif

#ifdef DS_MEAS
#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_spinlock, "spinlock", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_ticketlock, "ticketlock", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_spinlock_eb, "spinlock_backoff", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_ticketlock_eb, "ticketlock_backoff", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_mcslock, "mcs_lock", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_heap_ticket, "heap_ticket", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_heap_mcs, "heap_mcs", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_list, "list", rate_gap);
	}
#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_list_mcs, "list_mcs", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		meas_op(meas_stack, "stack", rate_gap);
	}

////////////////////////////////////////
#endif

	printc("Parallel benchmark: %d cores done\n", ncores);

	return;
}

int acap_corex[NUM_CPU], acap_corex_high[NUM_CPU];
volatile int calib_cpu, acap_core0;
volatile unsigned long long core0_e, corex_e[NUM_CPU];

int core0_high(void){
	assert(cos_cpuid() == 0);
	int ret, cli_acap, srv_acap;
	int spdid = cos_spd_id();

	ret = cos_async_cap_cntl(COS_ACAP_CREATE, spdid, spdid, cos_get_thd_id());
	cli_acap = ret >> 16;
	srv_acap = ret & 0xFFFF;

	acap_core0 = cli_acap;

	while (1) {
//		printc("core %d high thd %d, going to wait on acap %d\n", cos_cpuid(), cos_get_thd_id(), srv_acap);
		ret = cos_areceive(srv_acap);
//		printc("core 0 received high\n");
		core0_e = tsc_start();

//		printc("core 0 sending high\n");
		assert(acap_corex_high[calib_cpu]);
		cos_asend(acap_corex_high[calib_cpu]);
	}

	return 0;
}

int corex_low(void) {
	// next, for core x
	int ret, cli_acap, srv_acap, j;
	int cpu = cos_cpuid(), spdid = cos_spd_id();

	ret = cos_async_cap_cntl(COS_ACAP_CREATE, spdid, spdid, cos_get_thd_id());
	cli_acap = ret >> 16;
	srv_acap = ret & 0xFFFF;

	ck_pr_store_int(&acap_corex[cpu], cli_acap);

	unsigned long long results[10];
	int cnt = 0;

	unsigned long long s0;
	while (1) {
//		printc("core %d low thd %d, going to wait on acap %d\n", cos_cpuid(), cos_get_thd_id(), srv_acap);
		ret = cos_areceive(srv_acap);

//		printc("core 1 received 1st\n");

		corex_e[cpu] = 0;
		ck_pr_fence_strict_memory();

		s0 = tsc_start();

//		printc("core 1 sending 1st\n");
		cos_asend(acap_core0);
		while (corex_e[cpu] == 0);
		results[cnt++] = corex_e[cpu] - s0;
//		printc("core %d, round trip %llu\n", cpu, diff);
		if (cnt == 10) break;
	}

	for (j = 0; j < 10; j++) {
		printc("core %d, round trip %d: %llu\n", cpu, j, results[j]);
	}

	return 0;
}

int corex_high(void) {
	int ret, cli_acap, srv_acap;
	int cpu = cos_cpuid(), spdid = cos_spd_id();

	ret = cos_async_cap_cntl(COS_ACAP_CREATE, spdid, spdid, cos_get_thd_id());
	cli_acap = ret >> 16;
	srv_acap = ret & 0xFFFF;

	ck_pr_store_int(&acap_corex_high[cpu], cli_acap);
	
	while (1) {
//		printc("core %d high thd %d, going to wait on acap %d\n", cos_cpuid(), cos_get_thd_id(), srv_acap);
		ret = cos_areceive(srv_acap);
//		printc("core 1 high received\n");
		corex_e[cpu] = tsc_start();
	}

	return 0;
}

/* the variance of the cost of round-trip IPI from user spacing is
 * relatively large (from ~10k to ~11k). So not good for calibrating
 * TSC. */
int tsc_calibrate(void)
{
// core 0 calls here.
// curr prio is 10.
	assert(cos_cpuid() == 0);

	int spdid = cos_spd_id(), ret;
	int i,j;

	union sched_param sp;
	sp.c.type  = SCHEDP_PRIO;
	sp.c.value = 5;

	ret = cos_thd_create(core0_high, NULL, sp.v, 0, 0);
	while (acap_core0 == 0);

	unsigned long long s0, s1, results[10];
//	for (i = 1; i < NUM_CPU_COS; i++) {
	{       i = 1;
		calib_cpu = i;
		union sched_param sp, sp1;
		sp1.c.type = SCHEDP_CORE_ID;
		sp1.c.value = i;

		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = 9;
		cos_thd_create(corex_low, NULL, sp.v, sp1.v, 0);
		while (ck_pr_load_int(&acap_corex[i]) == 0);
		delay(100);

		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = 5;
		cos_thd_create(corex_high, NULL, sp.v, sp1.v, 0);
		while (ck_pr_load_int(&acap_corex_high[i]) == 0);
		delay(100);

                //////////////////////
		for (j = 0; j < 10; j++) {
			core0_e = 0;
			ck_pr_fence_strict_memory();

			s0 = tsc_start();
//			printc("core 0 sending 1st\n");
			cos_asend(acap_corex[i]);
			while (core0_e == 0);
			results[j]= core0_e - s0;
//			printc("core 0, round trip %llu\n", diff);
			delay(100);
		}
		for (j = 0; j < 10; j++) {
			printc("core 0, round trip %d: %llu\n", j, results[j]);
		}
	}

	return 0;
}

volatile int tsc_diff[NUM_CPU];

int tsc_calib_corex(void)
{
	int spdid = cos_spd_id(), ret;
	ret = cos_async_cap_cntl(9999, spdid, spdid, cos_cpuid());
	ck_pr_store_int((int *)(&(tsc_diff[cos_cpuid()])), ret);
//	printc("core %d-0, avg %d>>>>\n", cos_cpuid(), ret);
	
	return 0;
}

int tsc_calibrate_kern(void)
{
	int spdid = cos_spd_id();
	int i;
	union sched_param sp, sp1;
	int ret;
	
	for (i = 1; i < NUM_CPU_COS; i++) {
		sp1.c.type = SCHEDP_CORE_ID;
		sp1.c.value = i;

		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = 5;
		cos_thd_create(tsc_calib_corex, NULL, sp.v, sp1.v, 0);

		ret = cos_async_cap_cntl(9999, spdid, spdid, i);
//		printc("core 0-%d, avg %d>>>>\n", i, ret);
		delay(30);
		
		while (tsc_diff[i] == 0) ;
		/* if (ret == 9999 || tsc_diff[i] == 9999) { */
		/* 	i--; */
		/* 	tsc_diff[i] = 0; */
		/* 	printc("redo...\n"); */
		/* 	delay(50); */
		/* 	continue; */
		/* } */
		//printc("core 0-%d: avg %d ; %d-0: avg %d >>>>\n", i, ret, i, tsc_diff[i]);
		printc("core 0-%d: skew %d cycles >>>>\n", i, ret);
	}

	return 0;
}

#define PP_ITER 1024
int ping_pong(void)
{
	int i, ret;
	unsigned long long s,e;
	s = tsc_start();
	for (i = 0; i < PP_ITER; i++) {
		ret = call(i, i*2, i*i, i*10);
		printc("%d: ret %d\n", i, ret);
	}
	e = tsc_start();

	printc("pingpong avg %llu\n",(e-s)/PP_ITER);
	
	return 0;
}

static inline void
struct_init(void)
{
	int i;

	h = heap_alloc(NUM_CPU_COS, c, u);
	assert(h);

	for (i = 0 ; i < NUM_CPU_COS ; i++) {
		es[i].value = i;
//		assert(!heap_add(h, &es[i]));
	}

}

int meas(void)
{
	int i, j, omp_cores;
	int gap = 0;

	printc("Parallel benchmark in component %ld. ITER %llu\n", cos_spd_id(), (unsigned long long)ITER);
	mman_alias_page(cos_spd_id(), 1234, 7890, 9999, MAPPING_RW);

	struct_init();

#ifdef ENABLE_TDMA
	printc("TDMA window %d cycles, each slot %d cycles, num_slots %d, drift %d cycles.\n",
	       (int)TDMA_WINDOW, (int)TDMA_SLOT, (int)TDMA_NUM_SLOTS, (int)TDMA_DRIFT);
#endif

	/* No need to calibrate tsc. They are pretty synced. */
//	tsc_calibrate();
//	tsc_calibrate_kern();
//	ping_pong();
//	meas_op(meas_ptregs, "ptregs", 0);
	meas_op(null_op, "rdtsc_cost", 0);
	

#pragma omp parallel for
	for (i = 0; i < NUM_CPU_COS; i++) 
		if (i == 0) omp_cores = omp_get_num_threads();

	int rates[15] = {0,1,2,3,4,5,10,15,20,25,30,35,40,45,50};
	
//	for (gap = 0; gap <= (CPU_GHZ * 100 * 1000); gap += (10 * CPU_GHZ * 1000)) {

//	for (i = 0; i < 7; i++) {
//		gap = rates[i] * (CPU_GHZ*1000);
//	{       gap = GAP_US * (CPU_GHZ*1000);
	{       gap = 0;

#ifndef ENABLE_RATE_LIMIT 
		// no rate_limit!
		if (gap != 0) {
			printc("RATE limit disabled while rate_gap > 0!!\n");
			return -1;
		}
#endif
		rate_gap = gap;

		n_cores = 1;
		go_par(n_cores);

		int k;
		for (k = 5; k < omp_cores; k+=5) {
			n_cores = k;
			go_par(n_cores);
		}

		n_cores = omp_cores;
		go_par(n_cores);
	}

	/* rate_gap = 0; */
	/* n_cores = 1; */
	/* go_par(n_cores); */

	sched_block(cos_spd_id(), 999); //call to exit...

	return 0;
}

int main() {
#ifdef DISABLE 
	return 0;
#endif
	if (NUM_CPU_COS == 1) {
		printc("Par test but Composite only has 1 cpu. No parallel execution can be done.\n"
		       "NUM_CPU needs to be greater than 2 to enable parallel execution in Composite.\n");
	}
	
	union sched_param sp;
	
	sp.c.type  = SCHEDP_PRIO;
	sp.c.value = 10;

	cos_thd_create(meas, NULL, sp.v, 0, 0);

	return 0;
}
