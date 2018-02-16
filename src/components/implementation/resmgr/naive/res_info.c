#include "res_info.h"

static struct res_comp_info resci[MAX_NUM_COMPS];
static unsigned int res_comp_count;

struct res_comp_info *
res_info_comp_find(spdid_t sid)
{
	return &resci[sid];
}

unsigned int
res_info_count(void)
{
	return res_comp_count;
}

struct res_thd_info *
res_info_thd_find(struct res_comp_info *rci, thdid_t tid)
{
	int i = 0;

	assert(rci && rci->initflag);
	for (; i < rci->thd_used; i++) {
		if (((rci->tinfo[i]).schthd)->thdid == tid) return &(rci->tinfo[i]);
	}

	return NULL;
}

struct res_comp_info *
res_info_comp_init(spdid_t sid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
		   capid_t cap_frontier, vaddr_t heap_frontier, spdid_t psid)
{
	struct cos_compinfo *ci = cos_compinfo_get(&(resci[sid].defci));

	resci[sid].cid       = sid;
	resci[sid].sched_off = -1; /*TODO: sched & off */
	resci[sid].thd_used  = 1;
	resci[sid].parent    = &resci[psid];

	cos_meminfo_init(&ci->mi, 0, 0, 0);
	cos_compinfo_init(ci, pgtbl_cap, captbl_cap, compcap, heap_frontier, cap_frontier, 
			  cos_compinfo_get(cos_defcompinfo_curr_get()));
	resci[sid].initflag  = 1;
	res_comp_count ++;

	return &resci[sid];
}

struct res_thd_info *
res_info_thd_init(struct res_comp_info *rci, struct sl_thd *t)
{
	int off;

	assert(rci && rci->initflag);
	assert(rci->thd_used < MAX_NUM_THREADS-1);
	assert(t);

	off = rci->thd_used;
	rci->thd_used ++;

	rci->tinfo[off].schthd = t;

	return &(rci->tinfo[off]);
}

struct res_thd_info *
res_info_initthd_init(struct res_comp_info *rci, struct sl_thd *t)
{
	assert(rci && rci->initflag);
	assert(rci->thd_used < MAX_NUM_THREADS-1);
	assert(t);

	rci->tinfo[0].schthd = t;

	return &(rci->tinfo[0]);

}

struct res_thd_info *
res_info_initthd(struct res_comp_info *rci)
{
	assert(rci);

	return &(rci->tinfo[0]);
}

void
res_info_init(void)
{
	res_comp_count = 0;
	memset(resci, 0, sizeof(struct res_comp_info)*MAX_NUM_COMPS);
}
