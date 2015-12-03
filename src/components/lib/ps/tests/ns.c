#include <stdio.h>
#include <stdlib.h>

#include <ps_ns.h>
#include <ps_plat_linux.h>

/* 
 * FIXME: 
 * - segfault when ns is not allocated (i.e. when default ns is used)
 */

#define LEAF_ORDER 7
PS_NSSLAB_CREATE(nstest, sizeof(void *), 3, 9, LEAF_ORDER)

#define SMRITER (2737*128)
#define SLABITER SMRITER
void *ds[SMRITER];
ps_desc_t descs[SMRITER];
struct parsec ps;

void
test_slab_alloc_lkup(void)
{
	int i;
	struct ps_ns *ns;

	ns = ps_nsptr_create_slab_nstest();
	assert(ns);
	
	printf("--------------------[  NS Slab Tests  ]-----------------\n");

	printf("Testing ps_ns slab allocation: objmem %lu (ns objmemsz %lu), sz = nobj %lu * %d\n", 
	       ps_slab_objmem_nstest(), __ps_slab_objmemsz(sizeof(void*)),
	       ps_slab_nobjs_nstest(), 1<<LEAF_ORDER);

	for (i = 0 ; i < SLABITER/2 ; i++)        assert((ds[i] = ps_nsptr_alloc_nstest(ns, &descs[i])));
	for (i = 0 ; i < SLABITER/4 ; i++)        ps_nsptr_free_nstest(ns, ds[i]);
	for (i = 0 ; i < SLABITER/4 ; i++)        assert((ds[i] = ps_nsptr_alloc_nstest(ns, &descs[i])));
	for (i = SLABITER/2 ; i < SLABITER ; i++) assert((ds[i] = ps_nsptr_alloc_nstest(ns, &descs[i])));
	for (i = 0 ; i < SLABITER ; i++)          ps_nsptr_free_nstest(ns, ds[i]);

	for (i = 0 ; i < SLABITER ; i++)          assert(!ps_nsptr_lkup_nstest(ns, descs[i]));
	for (i = 0 ; i < (1<<LEAF_ORDER)/2 ; i++) assert((ds[i] = ps_nsptr_alloc_nstest(ns, &descs[i])));
	for (i = 0 ; i < (1<<LEAF_ORDER)/2 ; i++) assert(ps_nsptr_lkup_nstest(ns, descs[i]) == ds[i]);
	for (i = (1<<LEAF_ORDER)/2 ; 
	     i < 1<<LEAF_ORDER ; i++)             assert(!ps_nsptr_lkup_nstest(ns, descs[i]));
	for (i = 0 ; i < (1<<LEAF_ORDER)/2 ; i++) ps_nsptr_free_nstest(ns, ds[i]);
	for (i = 0 ; i < SLABITER ; i++)          assert((ds[i] = ps_nsptr_alloc_nstest(ns, &descs[i])));
	for (i = 0 ; i < SLABITER ; i++)          assert(ps_nsptr_lkup_nstest(ns, descs[i]) == ds[i]);
	for (i = 0 ; i < SLABITER ; i++)          ps_nsptr_free_nstest(ns, ds[i]);

	printf("--------------------[ NS Slab: SUCCESS ]-----------------\n");
}

PS_NS_CREATE(nstest2, sizeof(void *), 3, 9, LEAF_ORDER)

void
test_smr_alloc_lkup(void)
{
	int i;
	struct ps_ns *ns;

	ns = ps_nsptr_create_nstest(&ps);
	assert(ns);
	
	printf("--------------------[  NS Tests  ]-----------------\n");

	printf("Testing ps_ns allocation: objmem %lu (ns objmemsz %lu), sz = nobj %lu * %d\n", 
	       ps_slab_objmem_nstest2(), __ps_slab_objmemsz(sizeof(void*)),
	       ps_slab_nobjs_nstest2(), 1<<LEAF_ORDER);

	for (i = 0 ; i < SMRITER/2 ; i++)         assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
	for (i = 0 ; i < SMRITER/4 ; i++)         ps_nsptr_free_nstest2(ns, ds[i]);
	for (i = 0 ; i < SMRITER/4 ; i++)         assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
	for (i = SMRITER/2 ; i < SMRITER ; i++)   assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
	for (i = 0 ; i < SMRITER ; i++)           ps_nsptr_free_nstest2(ns, ds[i]);

	for (i = 0 ; i < SMRITER ; i++)           assert(!ps_nsptr_lkup_nstest2(ns, descs[i]));
	for (i = 0 ; i < (1<<LEAF_ORDER)/2 ; i++) assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
	for (i = 0 ; i < (1<<LEAF_ORDER)/2 ; i++) assert(ps_nsptr_lkup_nstest2(ns, descs[i]) == ds[i]);
	for (i = (1<<LEAF_ORDER)/2 ; 
	     i < 1<<LEAF_ORDER ; i++)             assert(!ps_nsptr_lkup_nstest2(ns, descs[i]));
	for (i = 0 ; i < (1<<LEAF_ORDER)/2 ; i++) ps_nsptr_free_nstest2(ns, ds[i]);
	for (i = 0 ; i < SMRITER ; i++)           assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
	for (i = 0 ; i < SMRITER ; i++)           assert(ps_nsptr_lkup_nstest2(ns, descs[i]) == ds[i]);
	for (i = 0 ; i < SMRITER ; i++)           ps_nsptr_free_nstest2(ns, ds[i]);

	printf("--------------------[ NS: SUCCESS ]-----------------\n");
}

#define ITER 128
volatile void *val;

void
test_perf(void)
{
	int i, j;
	struct ps_ns *ns;
	ps_desc_t d;
	u64_t start, end;

	ns = ps_nsptr_create_nstest(&ps);
	assert(ns);
	
	printf("--------------------[  NS Performance Tests  ]-----------------\n");

	ps_nsptr_alloc_nstest2(ns, &d);
	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < SMRITER ; i++) assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
		for (i = 0 ; i < SMRITER ; i++) ps_nsptr_free_nstest2(ns, ds[i]);
	}
	end = ps_tsc();
	printf("ns alloc+free average %lld\n", (end-start)/(ITER*SMRITER));

	for (i = 0 ; i < SMRITER ; i++) assert((ds[i] = ps_nsptr_alloc_nstest2(ns, &descs[i])));
	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < SMRITER ; i++) val = ps_nsptr_lkup_nstest2(ns, descs[i]);
	}
	end = ps_tsc();
	for (i = 0 ; i < SMRITER ; i++) ps_nsptr_free_nstest2(ns, ds[i]);
	printf("ns lookup average %lld\n", (end-start)/(ITER*SMRITER));

	printf("--------------------[ NS: SUCCESS ]-----------------\n");
}

void *
disassemble_lkup(struct ps_ns *ns)
{
	return ps_nsptr_lkup_nstest2(ns, descs[5]); /* arbitrary value...we never execute this. */
}

int
main(void)
{
	thd_set_affinity(pthread_self(), 0);
	ps_init(&ps);
	test_slab_alloc_lkup();
	test_smr_alloc_lkup();
	test_perf();

	return 0;
}
