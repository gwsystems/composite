#include <cos_types.h>
#include <memmgr.h>
#include <pongshmem.h>
#include <shm_bm.h>
#include <string.h>

shm_bm_t shm;

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
	shm = shm_bm_map(shmid);
	PRINTLOG(PRINT_DEBUG, "%s: Shared memory mapped in pong\n", (shm == 0) ? "FAILURE" : "SUCCESS");
}

void
pongshmem_test_objread(shm_bufid_t objid, int test_string)
{
	struct obj_test *obj;
	int              failure; 

	obj = (struct obj_test *) shm_bm_obj_use(shm, objid);
	PRINTLOG(PRINT_DEBUG, "%s: (%d) Pong can get shared object from buffer\n", (obj == 0) ? "FAILURE" : "SUCCESS", test_string+1);
	failure = strcmp(obj->string, ping_test_strings[test_string]) != 0;
	PRINTLOG(PRINT_DEBUG, "%s: (%d) Pong can read data from ping\n", (failure) ? "FAILURE" : "SUCCESS", test_string+1);

	strcpy(obj->string, pong_test_strings[test_string]);
}
