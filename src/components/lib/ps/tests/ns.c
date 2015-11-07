#include <stdio.h>

#include <ps_ns.h>

#define LEAF_ORDER 5

PS_NS_SLAB_CREATE(nstest, sizeof(void *), LEAF_ORDER)
PS_NS_PARSLAB_CREATE(nssmrtest, sizeof(void *), LEAF_ORDER)

#define ITER 273

void *ds[ITER];
struct parsec ps;

void
test_smralloc(void)
{
	int i;

	ps_init(&ps);
	ps_ns_init_nssmrtest(&ps);

	for (i = 0 ; i < ITER/2 ; i++) {
		ps_desc_t d;

		ds[i] = ps_ns_alloc_nssmrtest(&d);
	}
	for (i = 0 ; i < ITER/4 ; i++) ps_ns_free_nssmrtest(ds[i]);
	for (i = 0 ; i < ITER/4 ; i++) {
		ps_desc_t d;

		ds[i] = ps_ns_alloc_nssmrtest(&d);
	}
	for (i = ITER/2 ; i < ITER ; i++) {
		ps_desc_t d;

		ds[i] = ps_ns_alloc_nssmrtest(&d);
	}
	for (i = 0 ; i < ITER ; i++) ps_ns_free_nssmrtest(ds[i]);
}

void
test_alloc(void)
{
	int i;

	ps_ns_init_slab_nstest();
	printf("Testing ps_ns allocation: objmem %lu (ns objmemsz %lu), sz = nobj %lu * %d\n", 
	       ps_slab_objmem_nstest(), __ps_slab_objmemsz(sizeof(void*)),
	       ps_slab_nobjs_nstest(), 1<<LEAF_ORDER);

	for (i = 0 ; i < ITER/2 ; i++) {
		ps_desc_t d;

		ds[i] = ps_ns_alloc_nstest(&d);
	}
	for (i = 0 ; i < ITER/4 ; i++) ps_ns_free_nstest(ds[i]);
	for (i = 0 ; i < ITER/4 ; i++) {
		ps_desc_t d;

		ds[i] = ps_ns_alloc_nstest(&d);
	}
	for (i = ITER/2 ; i < ITER ; i++) {
		ps_desc_t d;

		ds[i] = ps_ns_alloc_nstest(&d);
	}
	for (i = 0 ; i < ITER ; i++) ps_ns_free_nstest(ds[i]);
}

int
main(void)
{
	test_alloc();
	test_smralloc();

	return 0;
}
