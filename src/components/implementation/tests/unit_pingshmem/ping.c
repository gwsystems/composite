#include <cos_types.h>
#include <pongshmem.h>
#include <shm_bm.h>
#include <string.h>

char *ping_test_strings[] = {
	"PING TEST 1",
	"PING TEST 2",
	"PING TEST 3",
	"PING TEST 4",
};

char *pong_test_strings[] = {
	"PONG TEST 1",
	"PONG TEST 2",
	"PONG TEST 3",
	"PONG TEST 4",
};

shm_bm_t shm;

static unsigned long 
rdpmc (unsigned long cntr)
{
	unsigned int low, high;

	asm volatile("rdpmc" : "=a" (low), "=d" (high) : "c" (cntr));

	return low | ((unsigned long)high) << 32;
}

void 
ping_test_objread(void)
{
	struct obj_test *obj;
	shm_bm_objid_t   objid = 0;
	int              i, failure; 

	/* reset memory for test */
	shm_bm_init_testobj(shm);

	for (i = 0; i < 4; i++) {
		/* allocate an object from shared mem buffer */
		obj = (struct obj_test *)shm_bm_alloc_testobj(shm, &objid);
		printc("%s: (%d) Ping can allocate object from shared buffer\n", (obj == NULL) ? "FAILURE" : "SUCCESS", i+1);
		
		/* send the obj to pong */
		strcpy(obj->string, ping_test_strings[i]);
		pongshmem_test_objread(objid, i);

		/* pong should have read and changed the object data */
		failure = strcmp(obj->string, pong_test_strings[i]) != 0;
		printc("%s: (%d) Ping can read data from pong\n", (failure) ? "FAILURE" : "SUCCESS", i+1);
	}
}

void
ping_test_bigalloc(void)
{
	struct obj_test *obj;
	shm_bm_objid_t   objid;
	int              i;
	int              failure = 0;

	/* reset memory for test */
	shm_bm_init_testobj(shm);

	for (i = 0; i < BENCH_ITER; i++) {
		if ((obj = shm_bm_alloc_testobj(shm, &objid)) == NULL) {
			failure = 1;
			goto done;
		}
		obj->id = i;
	}

	for (i = 0; i < BENCH_ITER; i++) {
		pongshmem_bench_objread((shm_bm_objid_t)i); 	
	}

done:
	printc("%s: Ping can allocate from big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
	
	failure = shm_bm_alloc_testobj(shm, &objid) != NULL;
	printc("%s: Ping can't allocate from full big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
}

void
ping_test_objfree(void)
{
	shm_bm_objid_t   objid;
	const char      *teststr = "test string";
	struct obj_test *obj1, *obj2;
	int              failure, i;

	/* reset memory for test */
	shm_bm_init_testobj(shm);

	/* sanity check, allocate and free an object */
	obj1 = shm_bm_alloc_testobj(shm, &objid);
	shm_bm_free_testobj(obj1);
	/* we should get the same object in the buffer */
	obj2 = shm_bm_alloc_testobj(shm, &objid);
	failure = obj1 != obj2 && obj1 != NULL;

	printc("%s: Ping can free an allocated obj from the buffer\n", (failure) ? "FAILURE" : "SUCCESS");
	shm_bm_free_testobj(obj2);

	/* allocate the whole buffer */
	failure = 0;
	for (i = 0; i < BENCH_ITER-2; i++) {
		if (shm_bm_alloc_testobj(shm, &objid) == NULL) {
			failure = 1;
			goto done;
		}
	}

	obj1 = shm_bm_alloc_testobj(shm, &objid);
	obj2 = shm_bm_alloc_testobj(shm, &objid);

	/* buffer should be full */
	if (shm_bm_alloc_testobj(shm, &objid) != NULL) {
		failure = 1;
		goto done;
	}

	shm_bm_free_testobj(obj1);
	shm_bm_free_testobj(obj2);

	/* should be 2 more free objects now */
	if (shm_bm_alloc_testobj(shm, &objid) == NULL) {
		failure = 1;
		goto done;
	}
	if (shm_bm_alloc_testobj(shm, &objid) == NULL) {
		failure = 1;
		goto done;
	}

	/* buffer should be full */
	if (shm_bm_alloc_testobj(shm, &objid) != NULL) {
		failure = 1;
		goto done;
	}

done:
	printc("%s: Ping can free objects in the buffer and reuse them\n", (failure) ? "FAILURE" : "SUCCESS");
}

void
ping_test_bigfree(void)
{
	struct obj_test **obj_ptrs;
	shm_bm_objid_t    objid;
	int               i, npages;
	int               failure = 0;

	/* 
	 * need some heap space to store all the pointer
	 * we are about to create (too big for the stack)
	 */
	npages = (sizeof(void *) * BENCH_ITER) / PAGE_SIZE + 1; 
	obj_ptrs = (struct obj_test **)memmgr_heap_page_allocn(npages);

	/* reset memory for test */
	shm_bm_init_testobj(shm);

	/* allocate whole buffer */
	for (i = 0; i < BENCH_ITER; i++) {
		if ((obj_ptrs[i] = shm_bm_alloc_testobj(shm, &objid)) == NULL) {
			failure = 1;
			goto done;
		}
		obj_ptrs[i]->id = i;
	}

	/* check the whole buffer */
	for (i = 0; i < BENCH_ITER; i++) {
		pongshmem_bench_objread((shm_bm_objid_t)i); 	
	}
	/* free the whole buffer */
	for (i = 0; i < BENCH_ITER; i++) {
		shm_bm_free_testobj(obj_ptrs[i]);
	}

	/* reallocate whole buffer */
	for (i = 0; i < BENCH_ITER; i++) {
		if ((obj_ptrs[i] = shm_bm_alloc_testobj(shm, &objid)) == NULL) {
			failure = 1;
			goto done;
		}
		obj_ptrs[i]->id = i;
	}

	/* recheck the whole buffer */
	for (i = 0; i < BENCH_ITER; i++) {
		pongshmem_bench_objread((shm_bm_objid_t)i); 	
	}


done:
	printc("%s: Ping can allocate and reallocate from big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
	
}

void 
ping_test_refcnt(void)
{
	shm_bm_objid_t   objid = 0;
	struct obj_test *obj, *obj2;
	int              i, failure; 
	const char      *teststr = "test string";

	/* reset memory for test */
	shm_bm_init_testobj(shm);

	failure = 0;

	/* allocate an object from the buffer */
	obj = shm_bm_alloc_testobj(shm, &objid); 
	strcpy(obj->string, teststr);

	/*
	 * pong gets a reference to object and frees it
	 * but the object should not be deallocated bc we 
	 * still hold a reference
	 */ 
	pongshmem_test_refcnt(objid);

	/*
	 * we still hold a reference to obj, so we should not
	 * should not get a reference to it when we allocate
	 * a new object
	 */
	obj2 = shm_bm_alloc_testobj(shm, &objid); 
	if (obj2 == obj) {
		failure = 1;
		goto done;
	}

	shm_bm_free_testobj(obj);

	/*
	 * now that we have also freed the object, we should 
	 * get a reference to the same object if we realloc
	 */
	obj2 = shm_bm_alloc_testobj(shm, &objid); 
	if (obj2 != obj) failure = 1;

done:
	printc("%s: Reference counts prevent freeing object in-use\n", (failure) ? "FAILURE" : "SUCCESS");
}

void
ping_bench_syncinv(void)
{
	ps_tsc_t         begin, end, bench;
	int              i;

	begin = ps_tsc();
	for (i = 0; i < BENCH_ITER; i++) {
		pongshmem_bench_syncinv((unsigned long) i);
	}
	end = ps_tsc();
	bench = (end - begin) / BENCH_ITER;
	printc("BENCHMARK Regular syncronous invocation: %llu cycles\n", bench);
}

void
ping_bench_msgpassing(void)
{
	shm_bm_objid_t   objid = 0;
	struct obj_test *obj;
	ps_tsc_t         begin, end, bench;
	int              i;

	/* reset memory for test */
	shm_bm_init_testobj(shm);

	/*
	 * Counting seems to slowdown execution by a not-significant amount of cycles.
	 * Not sure if this is a hardware thing of has to do with the virtualization of 
	 * the PMU.
	 * Comment out this line for a more consistant tsc read.
	 */ 
	cos_pmu_program_event_counter(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0x49, 0x0E);

	begin = ps_tsc();
	for (i = 0; i < BENCH_ITER; i++) {
		/* allocate an obj from shared mem */
		obj = shm_bm_alloc_testobj(shm, &objid);
		/* send obj to server, server borrows it */
		obj->id = objid;
		pongshmem_bench_objread(objid);
	}
	end = ps_tsc();
	bench = (end - begin) / BENCH_ITER;
	printc("BENCHMARK Message passing: %llu cycles, DTLB misses: %lu\n", bench, rdpmc(0));
}

int
main(void)
{
	void  *mem;
	cbuf_t id;
	size_t shmsz;

	shmsz = round_up_to_page(shm_bm_size_testobj());
	id = memmgr_shared_page_allocn_aligned(shmsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);
	shm = shm_bm_create_testobj(mem, shmsz);
	if (!shm) {
		printc("FAILURE: could not create shm from allocated memory\n");
		return 1;
	}
	shm_bm_init_testobj(shm);
	pongshmem_test_map(id);

	ping_test_objread();
	ping_test_bigalloc();
	ping_test_objfree();
	ping_test_bigfree();
	ping_test_refcnt();


	ping_bench_syncinv();
	ping_bench_msgpassing();
	printc("Counter: %lu\n", rdpmc(0));



	return 0;
}
