#ifndef COS_KERNEL_API_H
#define COS_KERNEL_API_H

/*
 * Copyright 2015, Qi Wang and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_component.h>
#include <cos_debug.h>
/* Types mainly used for documentation */
typedef capid_t sinvcap_t;
typedef capid_t sretcap_t;
typedef capid_t asndcap_t;
typedef capid_t arcvcap_t;
typedef capid_t thdcap_t;
typedef capid_t tcap_t;
typedef capid_t compcap_t;
typedef capid_t captblcap_t;
typedef capid_t pgtblcap_t;
typedef capid_t hwcap_t;

/* Memory source information */
struct cos_meminfo {
	vaddr_t untyped_ptr,      umem_ptr,      kmem_ptr;
	vaddr_t untyped_frontier, umem_frontier, kmem_frontier;
	pgtblcap_t pgtbl_cap;
};

/* Component captbl/pgtbl allocation information */
struct cos_compinfo {
	/* capabilities to higher-order capability tables (or -1) */
	capid_t pgtbl_cap, captbl_cap, comp_cap;
	/* the frontier of unallocated caps, and the allocated captbl range */
	capid_t cap_frontier, caprange_frontier;
	/* the frontier for each of the various sizes of capability */
	capid_t cap16_frontier, cap32_frontier, cap64_frontier;
	/* heap pointer equivalent, and range of allocated PTEs */
	vaddr_t vas_frontier, vasrange_frontier;
	/* the source of memory */
	struct cos_compinfo *memsrc; /* might be self-referential */
	struct cos_meminfo mi;	     /* only populated for the component with real memory */
};

void cos_compinfo_init(struct cos_compinfo *ci, pgtblcap_t pgtbl_cap, captblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier, struct cos_compinfo *ci_resources);
/*
 * This only needs be called on compinfos that are managing resources
 * (i.e. likely only one).  All of the capabilities will be relative
 * to this component's captbls.
 */
void cos_meminfo_init(struct cos_meminfo *mi, vaddr_t untyped_ptr, unsigned long untyped_sz, pgtblcap_t pgtbl_cap);
void cos_meminfo_alloc(struct cos_compinfo *ci, vaddr_t untyped_ptr, unsigned long untyped_sz);

/*
 * This uses the next three functions to allocate a new component and
 * correctly populate ci (allocating all resources from ci_resources).
 */
int         cos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, capid_t cap_frontier, vaddr_t entry, struct cos_compinfo *ci_resources);
captblcap_t cos_captbl_alloc(struct cos_compinfo *ci);
pgtblcap_t  cos_pgtbl_alloc(struct cos_compinfo *ci);
compcap_t   cos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry);

typedef void (*cos_thd_fn_t)(void *);
thdcap_t  cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data);
/* Create the initial (cos_init) thread */
thdcap_t  cos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp);
sinvcap_t cos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry);
arcvcap_t cos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t enotif);
asndcap_t cos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap);

void *cos_page_bump_alloc(struct cos_compinfo *ci);

capid_t cos_cap_cpy(struct cos_compinfo *dstci, struct cos_compinfo *srcci, cap_t srcctype, capid_t srccap);
int cos_cap_cpy_at(struct cos_compinfo *dstci, capid_t dstcap, struct cos_compinfo *srcci, capid_t srccap);

int cos_thd_switch(thdcap_t c);
#define CAP_NULL 0
sched_tok_t cos_sched_sync(void);
int cos_switch(thdcap_t c, tcap_t t, tcap_prio_t p, tcap_time_t r, arcvcap_t rcv, sched_tok_t stok);
int cos_thd_mod(struct cos_compinfo *ci, thdcap_t c, void *tls_addr); /* set tls addr of thd in captbl */

int cos_asnd(asndcap_t snd, int yield);
/* returns non-zero if there are still pending events (i.e. there have been pending snds) */
int cos_rcv(arcvcap_t rcv);
/* returns the same value as cos_rcv, but also information about scheduling events */
int cos_sched_rcv(arcvcap_t rcv, thdid_t *thdid, int *blocked, cycles_t *cycles);

int cos_introspect(struct cos_compinfo *ci, capid_t cap, unsigned long op);

vaddr_t cos_mem_alias(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src);
int cos_mem_alias_at(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src);
vaddr_t cos_mem_move(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src);
int cos_mem_move_at(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src);
int cos_mem_remove(pgtblcap_t pt, vaddr_t addr);

/* Tcap operations */
tcap_t cos_tcap_alloc(struct cos_compinfo *ci);
int cos_tcap_transfer(tcap_t src, arcvcap_t dst, tcap_res_t res, tcap_prio_t prio);
int cos_tcap_delegate(asndcap_t dst, tcap_t src, tcap_res_t res, tcap_prio_t prio, tcap_deleg_flags_t flags);
int cos_tcap_merge(tcap_t dst, tcap_t rm);

/* Hardware (interrupts) operations */
hwcap_t cos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap);
int cos_hw_attach(hwcap_t hwc, hwid_t hwid, arcvcap_t rcvcap);
int cos_hw_detach(hwcap_t hwc, hwid_t hwid);
void *cos_hw_map(struct cos_compinfo *ci, hwcap_t hwc, paddr_t pa, unsigned int len);
int cos_hw_cycles_per_usec(hwcap_t hwc);

/* Default API */
/* Capabilities for Async activation end point */
struct cos_aep_info {
	tcap_t    tc;
	thdcap_t  thd;
	arcvcap_t rcv;
};

/* Default Component information */
struct cos_defci {
	struct cos_compinfo ci;
	struct cos_aep_info sched_aep;
};

/* current component's default component information */
extern struct cos_defci curr_defci;

/*
 * cos_defci_init: initialize the current component's global cos_defci struct using the standard boot capabilities layout.
 */
void cos_defci_init(void);
/*
 * cos_defci_init_ext: initialize the current component's global cos_defci struct using the parameters passed.
 */
void cos_defci_init_ext(tcap_t sched_tc, thdcap_t sched_thd, arcvcap_t sched_rcv, pgtblcap_t pgtbl_cap, captblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier);
/*
 * cos_compinfo_get: returns the cos_compinfo pointer that points to the cos_compinfo struct inside cos_defci.
 */
struct cos_compinfo *cos_compinfo_get(void);
/*
 * cos_sched_aep_get: returns the AEP pointer that points to the current component's scheduler.
 */
struct cos_aep_info *cos_sched_aep_get(void);

/*
 * cos_defci_child_alloc: this is called to create a child component with new initial capabilities like pgtbl, captbl, compcap, etc.
 *                        if is_sched is set, scheduling end_point is created for the new component, 
 *                        else current component's scheduler is used as scheduler for the child component.
 */
int cos_defci_child_alloc(struct cos_defci *child_defci, vaddr_t entry, vaddr_t heap_ptr, capid_t cap_frontier, int is_sched);

/*
 * thread function that takes it's async rcv end-point as an argument along with user-passed void *
 */
typedef void (*cos_aepthd_fn_t)(arcvcap_t, void *);
/*
 * cos_aep_alloc: creates a new async activation end-point which includes thread, tcap and rcv capabilities. 
 */
int cos_aep_alloc(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data);
/*
 * cos_aep_alloc: creates a new async activation end-point, using an existing tcap. 
 */
int cos_aep_tcap_alloc(struct cos_aep_info *aep, tcap_t tc, cos_aepthd_fn_t fn, void *data);

/*
 * cos_defswitch: thread switch api using the default scheduling tcap and rcv.
 */
int cos_defswitch(thdcap_t c, tcap_prio_t p, tcap_time_t r, sched_tok_t stok);

#endif /* COS_KERNEL_API_H */
