#include <cos_types.h>
#include <memmgr.h>
#include <pongshmem.h>
#include <shm_bm.h>
#include <string.h>

shm_bm_t shm;

SHM_BM_CREATE(test, sizeof(struct obj_test), BENCH_ITER)

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
pongshmem_test_map(cbuf_t shmid)
{
	int ret = shm_bm_map_test(shmid);
	PRINTLOG(PRINT_DEBUG, "%s: Shared memory mapped in pong\n", (ret == 0) ? "FAILURE" : "SUCCESS");
}

void
pongshmem_test_objread(shm_objid_t objid, int test_string)
{
	struct obj_test *obj;
	int              failure; 

	/* get a reference to shared object sent from ping */
	obj = (struct obj_test *) shm_bm_obj_use_test(objid);
	PRINTLOG(PRINT_DEBUG, "%s: (%d) Pong can get shared object from buffer\n", (obj == 0) ? "FAILURE" : "SUCCESS", test_string+1);
	
	/* verify that we can read data from ping */
	failure = strcmp(obj->string, ping_test_strings[test_string]) != 0;
	PRINTLOG(PRINT_DEBUG, "%s: (%d) Pong can read data from ping\n", (failure) ? "FAILURE" : "SUCCESS", test_string+1);

	/* send new data to ping */
	strcpy(obj->string, pong_test_strings[test_string]);
}

void
pongshmem_test_refcnt(shm_objid_t objid)
{
	struct obj_test *obj;
	int              failure; 
	const char      *teststr = "test string";

	/* get a reference to shared object sent from ping */
	obj = (struct obj_test *) shm_bm_obj_use_test(objid);
	if (obj == 0) 
		PRINTLOG(PRINT_DEBUG, "FAILURE: Pong can get shared object from buffer\n");

	shm_bm_obj_free_test(obj);
}

void 
pongshmem_bench_map(cbuf_t shmid)
{
	shm_bm_map_test(shmid);
}

void 
pongshmem_bench_map_nonstatic(cbuf_t shmid)
{
	shm = shm_bm_map(shmid);
}

unsigned long global_word;

void
pongshmem_bench_syncinv(unsigned long word) 
{
	global_word = word;
}

void
pongshmem_bench_objread_nonstatic(shm_bufid_t objid)
{
	struct obj_test *obj;

	/* get a reference to shared object sent from ping */
	obj = (struct obj_test *) shm_bm_obj_take(shm, objid);
	(void)obj;
}

void
pongshmem_bench_objread(shm_objid_t objid)
{
	struct obj_test *obj;

	/* get a reference to shared object sent from ping */
	obj = (struct obj_test *) shm_bm_obj_borrow_test(objid);
	(void)obj;
}

