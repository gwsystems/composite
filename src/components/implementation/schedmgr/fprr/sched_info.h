#ifndef SCHED_INFO_H
#define SCHED_INFO_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>

#define SCHED_MAX_CHILD_COMPS 8

struct sched_childinfo {
	struct cos_defcompinfo defcinfo;
	struct sl_thd         *initthd;
	comp_flag_t            flags;
	spdid_t                id;
};

struct sched_childinfo *sched_childinfo_find(spdid_t spdid);
struct sched_childinfo *sched_childinfo_alloc(spdid_t id, compcap_t compcap, comp_flag_t flags);
int sched_num_child_get(void);
void sched_childinfo_init(void);

static inline struct cos_defcompinfo *
sched_child_defci_get(struct sched_childinfo *sci)
{
	if (sci) return &sci->defcinfo;

	return NULL;
}

static inline struct sl_thd *
sched_child_initthd_get(struct sched_childinfo *sci)
{
	if (sci) return sci->initthd;

	return NULL;
}

static inline void
sched_child_initthd_set(struct sched_childinfo *sci, struct sl_thd *t)
{
	if (sci) sci->initthd = t;
}

#endif /* SCHED_INFO_H */
