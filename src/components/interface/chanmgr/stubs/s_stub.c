#include <cos_stubs.h>
#include <chanmgr.h>

COS_SERVER_3RET_STUB(int, chanmgr_sync_resources)
{
	sched_blkpt_id_t full, empty;
	int ret;

	ret = chanmgr_sync_resources((chan_id_t)p0, &full, &empty);
	*r1 = (word_t)full;
	*r2 = (word_t)empty;

	return ret;
}

COS_SERVER_3RET_STUB(int, chanmgr_mem_resources)
{
	cbuf_t cb;
	void *mem;
	int ret;

	ret = chanmgr_mem_resources((chan_id_t)p0, &cb, &mem);
	*r1 = (word_t)cb;
	*r2 = (word_t)NULL; 		/* addr not meaningful across address spaces */

	return ret;
}
