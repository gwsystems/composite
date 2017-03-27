#include <stdio.h>
#include <stdlib.h>

#include <ps_slab.h>

#define SMALLSZ 1
#define LARGESZ 8000

struct small {
	char x[SMALLSZ];
};

struct larger {
	char x[LARGESZ];
};

PS_SLAB_CREATE_DEF(s, sizeof(struct small))
PS_SLAB_CREATE(l, sizeof(struct larger), PS_PAGE_SIZE * 128)
PS_SLAB_CREATE(hextern, sizeof(struct larger), PS_PAGE_SIZE * 128)

#define ITER       (1024)
#define SMALLCHUNK 2
#define LARGECHUNK 32

/* These are meant to be disassembled and inspected, to validate inlining/optimization */
void *
disassemble_alloc()
{ return ps_slab_alloc_l(); }
void
disassemble_free(void *m)
{ ps_slab_free_l(m); }

void
mark(char *c, int sz, char val)
{
	int i;

	for (i = 0 ; i < sz ; i++) c[i] = val;
}

void
chk(char *c, int sz, char val)
{
	int i;

	for (i = 0 ; i < sz ; i++) assert(c[i] == val);
}

struct small  *s[ITER];
struct larger *l[ITER];

#define RB_SZ   (1024 * 32)
#define RB_ITER (RB_SZ * 1024)

void * volatile ring_buffer[RB_SZ] PS_ALIGNED;

unsigned long long free_tsc, alloc_tsc;

void
consumer(void)
{
	struct small *s;
	unsigned long i;
	unsigned long long start, end, tot = 0;

	meas_barrier(2);

	for (i = 0 ; i < RB_ITER ; i++) {
		unsigned long off = i % RB_SZ;

		while (!ring_buffer[off]) ;
		s = ring_buffer[off];
		ring_buffer[off] = NULL;

		start = ps_tsc();
		ps_slab_free_s(s);
		end = ps_tsc();
		tot += end-start;
	}
	free_tsc = tot / RB_ITER;
}

void
producer(void)
{
	struct small *s;
	unsigned long i;
	unsigned long long start, end, tot = 0;

	meas_barrier(2);

	for (i = 0 ; i < RB_ITER ; i++) {
		unsigned long off = i % RB_SZ;
		
		while (ring_buffer[off]) ; 

		start = ps_tsc();
		s = ps_slab_alloc_s();
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

void
test_correctness(void)
{
	int i, j;

	printf("Starting mark & check for increasing numbers of allocations.\n");
	for (i = 0 ; i < ITER ; i++) {
		l[i] = ps_slab_alloc_l();
		mark(l[i]->x, sizeof(struct larger), i);
		for (j = i+1 ; j < ITER ; j++) {
			l[j] = ps_slab_alloc_l();
			mark(l[j]->x, sizeof(struct larger), j);
		}
		for (j = i+1 ; j < ITER ; j++) {
			chk(l[j]->x, sizeof(struct larger), j);
			ps_slab_free_l(l[j]);
		}
	}
	for (i = 0 ; i < ITER ; i++) {
		assert(l[i]);
		chk(l[i]->x, sizeof(struct larger), i);
		ps_slab_free_l(l[i]);
	}
}

void
test_perf(void)
{
	int i, j;
	unsigned long long start, end;

	printf("Slabs:\n"
	       "\tsmall: objsz %lu, objmem %lu, nobj %lu\n"
	       "\tlarge: objsz %lu, objmem %lu, nobj %lu\n"
	       "\tlarge+nohead: objsz %lu, objmem %lu, nobj %lu\n",
	       (unsigned long)sizeof(struct small),  (unsigned long)ps_slab_objmem_s(), (unsigned long)ps_slab_nobjs_s(),
	       (unsigned long)sizeof(struct larger), (unsigned long)ps_slab_objmem_l(), (unsigned long)ps_slab_nobjs_l(),
	       (unsigned long)sizeof(struct larger), (unsigned long)ps_slab_objmem_hextern(), (unsigned long)ps_slab_nobjs_hextern());

	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < LARGECHUNK ; i++) s[i] = ps_slab_alloc_l();
		for (i = 0 ; i < LARGECHUNK ; i++) ps_slab_free_l(s[i]);
	}
	end = ps_tsc();
	end = (end-start)/(ITER*LARGECHUNK);
	printf("Average cost of large slab alloc+free: %lld\n", end);

	ps_slab_alloc_s();
	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < SMALLCHUNK ; i++) s[i] = ps_slab_alloc_s();
		for (i = 0 ; i < SMALLCHUNK ; i++) ps_slab_free_s(s[i]);
	}
	end = ps_tsc();
	end = (end-start)/(ITER*SMALLCHUNK);
	printf("Average cost of small slab alloc+free: %lld\n", end);

	ps_slab_alloc_hextern();
	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < LARGECHUNK ; i++) s[i] = ps_slab_alloc_hextern();
		for (i = 0 ; i < LARGECHUNK ; i++) ps_slab_free_hextern(s[i]);
	}
	end = ps_tsc();
	end = (end-start)/(ITER*LARGECHUNK);
	printf("Average cost of extern slab header, large slab alloc+free: %lld\n", end);
}

void
stats_print(struct ps_mem *m)
{
	struct ps_slab_stats s;
	int i;

	printf("Stats for slab @ %p\n", (void*)m);
	ps_slabptr_stats(m, &s);
	for (i = 0 ; i < PS_NUMCORES ; i++) {
		printf("\tcore %d, slabs %ld, partial slabs %ld, nfree %ld, nremote %ld\n", 
		       i, s.percore[i].nslabs, s.percore[i].npartslabs, s.percore[i].nfree, s.percore[i].nremote);
	}
}

int
main(void)
{
	thd_set_affinity(pthread_self(), 0);

	test_perf();

	stats_print(&__ps_mem_l);
	stats_print(&__ps_mem_s);
	test_correctness();
	stats_print(&__ps_mem_l);
	test_remote_frees();
	stats_print(&__ps_mem_s);

	return 0;
}
