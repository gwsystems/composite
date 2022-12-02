#include <cos_component.h>
#include <cos_stubs.h>
#include <chanmgr.h>
#include <memmgr.h>

COS_CLIENT_STUB(int, chanmgr_sync_resources, chan_id_t id, sched_blkpt_id_t *full, sched_blkpt_id_t *empty)
{
	COS_CLIENT_INVCAP;
	word_t f, e;
	int ret;

	ret  = cos_sinv_2rets(uc, id, 0, 0, 0, &f, &e);
	*full  = (sched_blkpt_id_t)f;
	*empty = (sched_blkpt_id_t)e;

	return ret;
}

COS_CLIENT_STUB(int, chanmgr_mem_resources, chan_id_t id, cbuf_t *cb, void **mem)
{
	COS_CLIENT_INVCAP;
	word_t c, _tmp;
	int ret;

	ret  = cos_sinv_2rets(uc, id, 0, 0, 0, &c, &_tmp);
	*cb = (cbuf_t)c;
	if (ret < 0) return ret;
	/* Lets get our own mapping for the channel */
	if (memmgr_shared_page_map(*cb, (vaddr_t *)mem) == 0) return -CHAN_ERR_NOMEM;

	return ret;
}
