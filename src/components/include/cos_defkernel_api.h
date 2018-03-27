/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 *
 * This API is layered on top of the cos_kernel_api and simply makes
 * some assumptions about capability setup, but provides a much
 * simpler API to use for creating and using asynchronous end-points.
 *
 * The main assumptions this API makes are: the initial tcap provided
 * in the bootup protocol is used along with the initial thread as the
 * parent tcap that schedules all others.  The asynchronous receive
 * end-points are created as a package of receive end-point, thread to
 * receive from the end-point, and tcap (which can be provided as a
 * separate argument).
 *
 * This API provides a wrapper structure around the cos_compinfo
 * (cos_definfo) to include the information about the root scheduling
 * thread and tcap.
 */

#ifndef COS_DEFKERNEL_API_H
#define COS_DEFKERNEL_API_H

#include <cos_kernel_api.h>

/*
 * thread function that takes it's async rcv end-point as an argument along with user-passed void *
 */
typedef void (*cos_aepthd_fn_t)(arcvcap_t, void *);

/* Capabilities for Async activation end point */
struct cos_aep_info {
	tcap_t          tc;
	thdcap_t        thd;
	thdid_t         tid;
	arcvcap_t       rcv;
	cos_aepthd_fn_t fn;
	void *          data;
};

/* Default Component information */
struct cos_defcompinfo {
	union {
		spdid_t id;
		struct cos_compinfo ci;
	};
	struct cos_aep_info sched_aep[NUM_CPU];
};

static void
cos_aepthd_fn(void *data)
{
	struct cos_aep_info *aep_info = (struct cos_aep_info *)data;
	cos_aepthd_fn_t      aep_fn   = aep_info->fn;
	void *               fn_data  = aep_info->data;

	(aep_fn)(aep_info->rcv, fn_data);

	/* TODO: handling destruction */
	assert(0);
}

/* Only spdid is required when using manager interfaces. */
void cos_defcompinfo_childid_init(struct cos_defcompinfo *defci, spdid_t id);

/*
 * cos_defcompinfo_curr_get: returns the current component's cos_defcompinfo.
 */
struct cos_defcompinfo *cos_defcompinfo_curr_get(void);
/*
 * cos_compinfo_get: returns the cos_compinfo pointer that points to the cos_compinfo struct inside cos_defcompinfo.
 */
struct cos_compinfo *cos_compinfo_get(struct cos_defcompinfo *defci);
/*
 * cos_sched_aep_get: returns the sched aep info from the defcompinfo.
 */
struct cos_aep_info *cos_sched_aep_get(struct cos_defcompinfo *defci);
/*
 * cos_defcompinfo_init: initialize the current component's global cos_defcompinfo struct using the standard boot
 * capabilities layout.
 */
void cos_defcompinfo_init(void);
/*
 * cos_defcompinfo_init_ext: initialize the current component's global cos_defcompinfo struct using the parameters
 * passed.
 */
void cos_defcompinfo_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv, pgtblcap_t pgtbl_cap,
                              captblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier);

/* for AP cores */
void cos_defcompinfo_sched_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv);
void cos_defcompinfo_sched_init(void);

/*
 * cos_defcompinfo_child_alloc: called to create a new child component including initial capabilities like pgtbl,
 * captbl, compcap, aep. if is_sched is set, scheduling end-point will also be created for the child component, else,
 * the current component's scheduler will remain the scheduler for the child component.
 */
int cos_defcompinfo_child_alloc(struct cos_defcompinfo *child_defci, vaddr_t entry, vaddr_t heap_ptr,
                                capid_t cap_frontier, int is_sched);

/*
 * cos_aep_alloc: creates a new async activation end-point which includes thread, tcap and rcv capabilities.
 *                struct cos_aep_info passed in, must not be stack allocated.
 */
int cos_aep_alloc(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data);
/*
 * cos_aep_alloc: creates a new async activation end-point, using an existing tcap.
 *                struct cos_aep_info passed in, must not be stack allocated.
 */
int cos_aep_tcap_alloc(struct cos_aep_info *aep, tcap_t tc, cos_aepthd_fn_t fn, void *data);

/*
 * cos_initaep_alloc: create an initaep in the @child_dci and using sched->rcv as the parent, sets up cos_sched_ape_get(@child_dci) with the init capabilities.
 * 		      if @sched == NULL, use the current scheduler in cos_sched_aep_get(cos_defcompinfo_get_cur()).
 *                    if @is_sched == 0, creates only the init thread (does not need @sched parameter)
 */
int cos_initaep_alloc(struct cos_defcompinfo *child_dci, struct cos_aep_info *sched, int is_sched);
/*
 * cos_initaep_tcap_alloc: same as cos_initaep_alloc with is_sched == 1, except it doesn't create a new tcap,
 *			   uses the tcap passed in @tc.
 */
int cos_initaep_tcap_alloc(struct cos_defcompinfo *child_dci, tcap_t tc, struct cos_aep_info *sched);

/*
 * cos_aep_alloc_ext: creates a new async activation end-point which includes thread, tcap and rcv capabilities in the child_dci component using sched_aep->rcv.
 *		      if @child_dci == NULL, create in the current component.
 */
int cos_aep_alloc_ext(struct cos_aep_info *aep, struct cos_defcompinfo *child_dci, struct cos_aep_info *sched_aep, thdclosure_index_t idx);

/*
 * cos_aep_alloc_ext: creates a new async activation end-point which includes thread, tcap and rcv capabilities in the child_dci component using sched_aep->rcv.
 *		      if @child_dci == NULL, create in the current component.
 */
int cos_aep_tcap_alloc_ext(struct cos_aep_info *aep, struct cos_defcompinfo *child_dci, struct cos_aep_info *sched_aep, tcap_t tc, thdclosure_index_t idx);

/*
 * cos_defswitch: thread switch api using the default scheduling tcap and rcv.
 */
int cos_defswitch(thdcap_t c, tcap_prio_t p, tcap_time_t r, sched_tok_t stok);

#endif /* COS_DEFKERNEL_API_H */
