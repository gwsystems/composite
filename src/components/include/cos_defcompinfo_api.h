#ifndef COS_DEFCOMPINFO_API_H
#define COS_DEFCOMPINFO_API_H

/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_kernel_api.h>

/*
 * thread function that takes it's async rcv end-point as an argument along with user-passed void *
 */
typedef void (*cos_aepthd_fn_t)(arcvcap_t, void *);

/* Capabilities for Async activation end point */
struct cos_aep_info {
	tcap_t          tc;
	thdcap_t        thd;
	arcvcap_t       rcv;
	cos_aepthd_fn_t fn;
	void           *data;
};

/* Default Component information */
struct cos_defcompinfo {
	struct cos_compinfo ci;
	struct cos_aep_info sched_aep;
};

/*
 * cos_defcompinfo_get: returns the current component's cos_defcompinfo.
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
 * cos_defcompinfo_init: initialize the current component's global cos_defcompinfo struct using the standard boot capabilities layout.
 */
void cos_defcompinfo_init(void);
/*
 * cos_defcompinfo_init_ext: initialize the current component's global cos_defcompinfo struct using the parameters passed.
 */
void cos_defcompinfo_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv, pgtblcap_t pgtbl_cap, captblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier);

/*
 * cos_defcompinfo_child_alloc: called to create a new child component including initial capabilities like pgtbl, captbl, compcap, aep.
 *                        if is_sched is set, scheduling end-point will also be created for the child component,
 *                        else, the current component's scheduler will remain the scheduler for the child component.
 */
int cos_defcompinfo_child_alloc(struct cos_defcompinfo *child_defci, vaddr_t entry, vaddr_t heap_ptr, capid_t cap_frontier, int is_sched);

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
 * cos_defswitch: thread switch api using the default scheduling tcap and rcv.
 */
int cos_defswitch(thdcap_t c, tcap_prio_t p, tcap_time_t r, sched_tok_t stok);

#endif /* COS_DEFCOMPINFO_API_H */
