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

/* Memory source information */
struct cos_meminfo {
	vaddr_t       umem_ptr,   untyped_ptr;
	unsigned long umem_range, untyped_range;
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

void cos_compinfo_init(struct cos_compinfo *ci, captblcap_t pgtbl_cap, pgtblcap_t captbl_cap, compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier, struct cos_compinfo *ci_resources);
/*
 * This only needs be called on compinfos that are managing resources
 * (i.e. likely only one).  All of the capabilities will be relative
 * to this component's captbls.
 */
void cos_meminfo_init(struct cos_meminfo *mi, vaddr_t umem_ptr, unsigned long umem_sz, vaddr_t untyped_ptr, unsigned long untyped_sz);

/*
 * This uses the next three functions to allocate a new component and
 * correctly populate ci (allocating all resources from ci_resources).
 */
int cos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, vaddr_t entry, struct cos_compinfo *ci_resources);
captblcap_t cos_captbl_alloc(struct cos_compinfo *ci);
pgtblcap_t cos_pgtbl_alloc(struct cos_compinfo *ci);
compcap_t cos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry);

typedef void (*cos_thd_fn_t)(void *);
thdcap_t  cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data);
/* Create the initial (cos_init) thread */
thdcap_t  cos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp);
sinvcap_t cos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry);
arcvcap_t cos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, compcap_t compcap);
asndcap_t cos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap);

void *cos_page_bump_alloc(struct cos_compinfo *ci);

int cos_thd_switch(thdcap_t c);
int cos_asnd(asndcap_t snd);
int cos_rcv(arcvcap_t rcv);

int cos_mem_alias(pgtblcap_t ptdst, vaddr_t dst, pgtblcap_t ptsrc, vaddr_t src);
int cos_mem_move(pgtblcap_t ptdst, vaddr_t dst, pgtblcap_t ptsrc, vaddr_t src);
int cos_mem_remove(pgtblcap_t pt, vaddr_t addr);

#endif /* COS_KERNEL_API_H */
