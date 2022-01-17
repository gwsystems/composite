#include <cos_types.h>
#include <memmgr.h>
#include <pongshmem.h>
#include <shm_bm.h>
#include <string.h>

#define BENCH_ITER 2048

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

void 
ping_test_objread(void)
{
	shm_bm_t         shm;
	struct obj_test *obj;
	shm_bufid_t      objid;
	int              i, failure; 
	cbuf_t           id;

	id = shm_bm_create(&shm, sizeof(struct obj_test), 4 * sizeof (struct obj_test));
	PRINTLOG(PRINT_DEBUG, "%s: Shared memory allocation in ping\n", (id == 0) ? "FAILURE" : "SUCCESS");

	pongshmem_test_map(id);

	for (i = 0; i < 4; i++) {
		// allocate an object from shared mem buffer
		obj = (struct obj_test *) shm_bm_obj_alloc(shm, &objid);
		PRINTLOG(PRINT_DEBUG, "%s: (%d) Ping can allocate object from shared buffer\n", (obj == 0) ? "FAILURE" : "SUCCESS", i+1);
		
		// send the obj to pong
		strcpy(obj->string, ping_test_strings[i]);
		pongshmem_test_objread(objid, i);

		// pong should have read and changed the object data
		failure = strcmp(obj->string, pong_test_strings[i]) != 0;
		PRINTLOG(PRINT_DEBUG, "%s: (%d) Ping can read data from pong\n", (failure) ? "FAILURE" : "SUCCESS", i+1);
	}

	// we should have allocated the entire buffer now
	failure = shm_bm_obj_alloc(shm, &objid) != 0;
	PRINTLOG(PRINT_DEBUG, "%s: Ping can't allocate from full shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
}

void
ping_test_bigalloc(void)
{
	shm_bm_t    shm;
	shm_bufid_t objid;
	int         i;
	int         failure = 0;
	int         big_num = 10000;

	shm_bm_create(&shm, sizeof(struct obj_test), big_num * sizeof (struct obj_test));
	for (i = 0; i < big_num; i++) {
		if (shm_bm_obj_alloc(shm, &objid) == 0) {
			failure = 1;
			break;
		}
	}
	PRINTLOG(PRINT_DEBUG, "%s: Ping can allocate from big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
	
	failure = shm_bm_obj_alloc(shm, &objid) != 0;
	PRINTLOG(PRINT_DEBUG, "%s: Ping can't allocate from full big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
}

void
ping_test_objfree(void)
{
	shm_bm_t         shm;
	shm_bufid_t      objid;
	const char      *teststr = "test string";
	struct obj_test *obj1, *obj2;
	int              failure, i;

	shm_bm_create(&shm, sizeof (struct obj_test), 20 * sizeof (struct obj_test));

	// sanity check, allocate and free and object
	obj1 = shm_bm_obj_alloc(shm, &objid);
	shm_bm_obj_free(obj1);
	// we should get the same object in the buffer 
	obj2 = shm_bm_obj_alloc(shm, &objid);
	failure = obj1 != obj2;
	PRINTLOG(PRINT_DEBUG, "%s: Ping can free an allocated obj from the buffer\n", (failure) ? "FAILURE" : "SUCCESS");
	shm_bm_obj_free(obj2);

	// allocate the whole buffer
	failure = 0;
	for (i = 0; i < 18; i++) {
		if (shm_bm_obj_alloc(shm, &objid) == 0)
			failure = 1;
	}

	obj1 = shm_bm_obj_alloc(shm, &objid);
	obj2 = shm_bm_obj_alloc(shm, &objid);

	// buffer should be full
	if (shm_bm_obj_alloc(shm, &objid) != 0) failure = 1;
	if (shm_bm_obj_alloc(shm, &objid) != 0) failure = 1;

	shm_bm_obj_free(obj1);
	shm_bm_obj_free(obj2);

	// should be 2 more free objects now
	if (shm_bm_obj_alloc(shm, &objid) == 0) failure = 1;
	if (shm_bm_obj_alloc(shm, &objid) == 0) failure = 1;

	// buffer should be full
	if (shm_bm_obj_alloc(shm, &objid) != 0) failure = 1;
	if (shm_bm_obj_alloc(shm, &objid) != 0) failure = 1;


	PRINTLOG(PRINT_DEBUG, "%s: Ping can free objects in the buffer and reuse them\n", (failure) ? "FAILURE" : "SUCCESS");
}

void 
ping_test_refcnt(void)
{
	shm_bm_t         shm;
	struct obj_test *obj, *obj2;
	shm_bufid_t      objid;
	int              i, failure; 
	cbuf_t           id;
	const char      *teststr = "test string";

	failure = 0;
	id = shm_bm_create(&shm, sizeof (struct obj_test), 4 * sizeof (struct obj_test));
	pongshmem_test_map(id);

	// allocate an object from the buffer
	obj = shm_bm_obj_alloc(shm, &objid); 
	strcpy(obj->string, teststr);

	// pong gets a reference to object and frees it
	// but the object should not be deallocated bc we 
	// still hold a reference
	pongshmem_test_refcnt(objid);

	// we still hold a reference to obj, so we should not
	// should not get a reference to it when we allocate
	// a new object
	obj2 = shm_bm_obj_alloc(shm, &objid);
	if (obj2 == obj) failure = 1;

	shm_bm_obj_free(obj);

	// now that we have also freed the object, we should 
	// get a reference to the same object if we realloc
	obj2 = shm_bm_obj_alloc(shm, &objid);
	if (obj2 != obj) failure = 1;
	
	PRINTLOG(PRINT_DEBUG, "%s: Reference counts prevent freeing object in-use\n", (failure) ? "FAILURE" : "SUCCESS");
}

void
ping_bench_msgpassing(void)
{
	shm_bm_t         shm;
	cbuf_t           id;
	shm_bufid_t      objid;
	struct obj_test *obj;
	ps_tsc_t         begin, end, bench;
	int              i;

	id = shm_bm_create(&shm, sizeof (struct obj_test), BENCH_ITER * sizeof (struct obj_test));
	pongshmem_bench_map(id);

	begin = ps_tsc();
	for (i = 0; i < BENCH_ITER; i++) {
		// allocate an obj from shared mem
		obj = (struct obj_test *) shm_bm_obj_alloc(shm, &objid);
		// send obj to server, server gets ref and frees
		pongshmem_bench_objread(objid);
		// free obj
		//shm_bm_obj_free(obj);
	}
	end = ps_tsc();
	bench = (end - begin) / BENCH_ITER;
	PRINTLOG(PRINT_DEBUG, "BENCHMARK Message passing with free: %llu cycles\n", bench);
}

int
main(void)
{
	ping_test_objread();
	ping_test_bigalloc();
	ping_test_objfree();
	ping_test_refcnt();

	ping_bench_msgpassing();

	return 0;
}
