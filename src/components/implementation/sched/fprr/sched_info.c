#include <cos_kernel_api.h>
#include <ps.h>
#include <sched_info.h>

#define SCHED_MAX_CHILD_COMPS 8
static struct sched_childinfo childinfo[SCHED_MAX_CHILD_COMPS];
static int sched_num_child = 0;

struct sched_childinfo *
sched_childinfo_find(spdid_t id)
{
	int i;

	for (i = 0; i < sched_num_child; i ++) {
		if (childinfo[i].id == id) return &(childinfo[i]);
	}

	return NULL;
}

struct sched_childinfo *
sched_childinfo_alloc(spdid_t id, compcap_t compcap, comp_flag_t flags)
{
	int idx = 0;
	struct sched_childinfo *sci = NULL;
	struct cos_defcompinfo *dci = NULL;

	assert(sched_num_child < SCHED_MAX_CHILD_COMPS - 1);
	idx = ps_faa((unsigned long *)&sched_num_child, 1);
	sci = &childinfo[idx];
	dci = sched_child_defci_get(sci);

	if (compcap) {
		struct cos_compinfo *ci = cos_compinfo_get(dci);

		ci->comp_cap = compcap;
	} else {
		cos_defcompinfo_childid_init(dci, id);
	}
	sci->id = id;

	return sci;
}

int
sched_num_child_get(void)
{
	return sched_num_child;
}

void
sched_childinfo_init(void)
{
	memset(childinfo, 0, sizeof(struct sched_childinfo) * SCHED_MAX_CHILD_COMPS);
}
