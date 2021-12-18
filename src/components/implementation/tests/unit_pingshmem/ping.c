#include <cos_types.h>
#include <memmgr.h>
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

void 
ping_test_objread(void)
{
    shm_bm_t         shm;
	struct obj_test *obj;
	shm_bufid_t      objid;
	int              i, failure; 
	cbuf_t           id;

	id = shm_bm_create(&shm, sizeof(struct obj_test), 4 * sizeof(struct obj_test));
	PRINTLOG(PRINT_DEBUG, "%s:Shared memory allocation in ping\n", (id == 0) ? "FAILURE" : "SUCCESS");

	pongshmem_test_map(id);

	for (i = 0; i < 4; i++) {
		obj = shm_bm_obj_alloc(shm, &objid);
		PRINTLOG(PRINT_DEBUG, "%s: (%d) Ping can allocate object from shared buffer\n", (obj == 0) ? "FAILURE" : "SUCCESS", i+1);
		
		strcpy(obj->string, ping_test_strings[i]);
		pongshmem_test_objread(objid, i);

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
	shm_bm_t shm;
	shm_bufid_t      objid;
	int      i, failure = 0;

	shm_bm_create(&shm, sizeof(struct obj_test), 200 * sizeof(struct obj_test));
	for (i = 0; i < 200; i++) {
		if (shm_bm_obj_alloc(shm, &objid) == 0) {
			failure = 1;
			break;
		}
	}
	PRINTLOG(PRINT_DEBUG, "%s: Ping can allocate from big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
	
	failure = shm_bm_obj_alloc(shm, &objid) != 0;
	PRINTLOG(PRINT_DEBUG, "%s: Ping can't allocate from full big shared buffer\n", (failure) ? "FAILURE" : "SUCCESS");
}

int
main(void)
{
	ping_test_objread();
	ping_test_bigalloc();

	return 0;
}
