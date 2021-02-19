#include <cos_component.h>
#include <llprint.h>
#include <static_slab.h>
#include <ps_list.h>

/***
 * A version of scheduling using a simple periodic timeout, and fixed
 * priority, round-robin scheduling.
 */

#include <slm.h>
#include <quantum.h>
#include <fprr.h>
#include <slm_api.h>

/*
 * The thread structure that is a container including
 *
 * - core slm thread
 * - scheduling policy data
 * - timer policy data
 * - resources allocated from the kernel via `crt`
 */
struct slm_thd_container {
	struct slm_thd thd;
	struct slm_sched_thd sched;
	struct slm_timer_thd timer;
	struct crt_thd resources;
};

struct slm_timer_thd *
slm_thd_timer_policy(struct slm_thd *t)
{
	return ps_container(t, struct slm_thd_container, thd)->timer;
}

struct slm_sched_thd *
slm_thd_sched_policy(struct slm_thd *t)
{
	return ps_container(t, struct slm_thd_container, thd)->sched;
}

struct slm_thd *
slm_thd_from_timer(struct slm_timer_thd *t)
{
	return ps_container(t, struct slm_thd_container, timer)->thd;
}

struct slm_thd *
slm_thd_from_sched(struct slm_sched_thd *t)
{
	return ps_container(t, struct slm_thd_container, sched)->thd;
}

SS_STATIC_SLAB(thd, struct slm_thd_container, MAX_NUM_THREADS);

/* Implementation for use by the other parts of the slm */
struct slm_thd *
slm_thd_lookup(thdid_t id)
{
	return ss_thd_get(id);
}

struct slm_thd_container *
sched_thd_alloc(void)
{
	struct slm_thd_container *t = ss_thd_
}

int
main(void)
{

}

void
cos_init(void)
{

}
