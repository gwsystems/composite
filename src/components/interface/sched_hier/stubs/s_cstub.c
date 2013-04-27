#include <sched_hier.h>

int 
__sg_sched_child_get_evt(spdid_t spdid, int idle, int wakediff, int __pad, int *off_len)
{
	int cevt, ret;
	unsigned short int tid;

	ret = sched_child_get_evt(spdid, idle, wakediff, &cevt, &tid, (u32_t*)&off_len[1]);
	off_len[0] = cevt << 16 | tid;
	return ret;
}
