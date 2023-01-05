#ifndef COS_KERNEL_API_H
#define COS_KERNEL_API_H

/*
 * Copyright 2015, Qi Wang and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 *
 * This library is a very light wrapper around the system call API.
 * As there is only a single system call (capability invocation), this
 * provides structure to the system calls.  Second, it abstracts one
 * of the most mundane aspects of the system: the management of
 * retyping memory, and of construction of resource tables
 * (page-tables and capability-tables).  It manages the capability and
 * virtual address name-spaces, and assumes that all untyped memory is
 * at a given range of addresses.
 *
 * This library is designed to provide the most functionality without
 * maintaining any fine-grained data-structures (no lists, no arrays,
 * etc...).  Instead, cos_compinfos are created for each component
 * (one for us), and all of the namespaces are tracked within that
 * (statically-sized) structure.  Additionally, a cos_meminfo holds
 * all of the information necessary to allocate memory (of all types).
 * It is likely that we will use the cos_meminfo associated with the
 * cos_compinfo for us (as we have all of the memory), while most
 * other components (i.e. that we load) do *not* have access to
 * memory, thus we shouldn't look in their page-table for memory to
 * use for allocations.
 *
 * For this library to use only static data-structures, we use
 * bump-pointers for managing allocation of each of the namespaces.
 * This means that we *never* deallocate resources (capabilities,
 * kernel resources, virtual addresses, etc...), thus never reuse
 * resources.  Thus this is quite limited in applicability.  However,
 * most embedded systems avoid dynamic allocation, making the
 * simplicity of this abstraction ideally suited to those systems.  It
 * can also be seen as a backend for allocation to layer other
 * allocators on top (that support deallocation).
 *
 * See the micro_booter for an examples of using this API.
 */

#include <cos_component.h>
#include <cos_debug.h>
#include <ps_plat.h>
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
typedef capid_t ulkcap_t;

/* Memory source information */
struct cos_meminfo {
	vaddr_t    untyped_ptr, umem_ptr, kmem_ptr;
	vaddr_t    untyped_frontier, umem_frontier, kmem_frontier;
	pgtblcap_t pgtbl_cap;

	capid_t	   second_lvl_pgtbl_cap;
	vaddr_t	   second_lvl_pgtbl_addr;
};

/* Component captbl/pgtbl allocation information */
struct cos_compinfo {
	/* capabilities to higher-order capability tables (or -1) */
	capid_t pgtbl_cap, captbl_cap, comp_cap;
	/* the frontier of unallocated caps, and the allocated captbl range */
	capid_t cap_frontier, caprange_frontier;
	/* the frontier for each of the various sizes of capability per core! */
	capid_t cap16_frontier[NUM_CPU], cap32_frontier[NUM_CPU], cap64_frontier[NUM_CPU];
	/* heap pointer equivalent, and range of allocated PTEs */
	vaddr_t vas_frontier;
	vaddr_t vasrange_frontier[COS_PGTBL_DEPTH - 1]; 
	/* the source of memory */
	struct cos_compinfo *memsrc; /* might be self-referential */
	struct cos_meminfo   mi;     /* only populated for the component with real memory */

	struct ps_lock cap_lock, mem_lock; /* locks to make the cap frontier and mem frontier updates and expands atomic */
	struct ps_lock va_lock; /* lock to make the vas frontier and bump expands for vas atomic */
	/* shared comp cap */
	capid_t comp_cap_shared;
	capid_t pgtbl_cap_shared;
};

void cos_compinfo_init(struct cos_compinfo *ci, pgtblcap_t pgtbl_cap, captblcap_t captbl_cap, compcap_t comp_cap,
                       vaddr_t heap_ptr, capid_t cap_frontier, struct cos_compinfo *ci_resources);/*
 * This only needs be called on compinfos that are managing resources
 * (i.e. likely only one).  All of the capabilities will be relative
 * to this component's captbls.
 */
void cos_meminfo_init(struct cos_meminfo *mi, vaddr_t untyped_ptr, unsigned long untyped_sz, pgtblcap_t pgtbl_cap);
void cos_meminfo_alloc(struct cos_compinfo *ci, vaddr_t untyped_ptr, unsigned long untyped_sz);
/* expand *only* the pgtbl-internal nodes */
vaddr_t cos_pgtbl_intern_alloc(struct cos_compinfo *ci, pgtblcap_t cipgtbl, vaddr_t mem_ptr, unsigned long mem_sz);
/*
 * Expand the page-table with a node at lvl, and return the pgtbl
 * capability to that node.  This also adjusts the frontier, so it
 * should be set to round_to_pgd_page(mem_ptr) before being called.
 */
pgtblcap_t cos_pgtbl_intern_expand(struct cos_compinfo *ci, vaddr_t mem_ptr, int lvl);
/*
 * Use a given pgtbl internal node to expand ci's page-table.  Adjusts
 * frontier as above.
 */
int cos_pgtbl_intern_expandwith(struct cos_compinfo *ci, pgtblcap_t intern, vaddr_t mem);

int cos_comp_alloc_shared(struct cos_compinfo *ci_og, pgtblcap_t ptc, vaddr_t entry, struct cos_compinfo *ci_resources, prot_domain_t protdom);

/*
 * This uses the next three functions to allocate a new component and
 * correctly populate ci (allocating all resources from ci_resources).
 */
int         cos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, capid_t cap_frontier, vaddr_t entry,
                               struct cos_compinfo *ci_resources, prot_domain_t protdom);
captblcap_t cos_captbl_alloc(struct cos_compinfo *ci);
pgtblcap_t  cos_pgtbl_alloc(struct cos_compinfo *ci);
compcap_t   cos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry, prot_domain_t protdom);

void       cos_ulk_info_init(struct cos_compinfo *ci);
pgtblcap_t cos_ulk_pgtbl_create(struct cos_compinfo *ci, pgtblcap_t *secondlvl);
ulkcap_t   cos_ulk_page_alloc(struct cos_compinfo *ci, pgtblcap_t ulkpt, vaddr_t uaddr);
int        cos_ulk_map_in(pgtblcap_t ptc);

void cos_comp_capfrontier_update(struct cos_compinfo *ci, capid_t cap_frontier, int try_expand);

typedef void (*cos_thd_fn_t)(void *);
thdcap_t cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data);
thdcap_t cos_thd_alloc_ext(struct cos_compinfo *ci, compcap_t comp, thdclosure_index_t idx);
/* Create the initial (cos_init) thread */
thdcap_t  cos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp);

sinvcap_t cos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry, invtoken_t token);
arcvcap_t cos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t enotif);
asndcap_t cos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap);

void *cos_page_bump_alloc(struct cos_compinfo *ci);
void *cos_page_bump_allocn(struct cos_compinfo *ci, size_t sz);
void *cos_page_bump_allocn_aligned(struct cos_compinfo *ci, size_t sz, size_t align);

capid_t cos_cap_cpy(struct cos_compinfo *dstci, struct cos_compinfo *srcci, cap_t srcctype, capid_t srccap);
int     cos_cap_cpy_at(struct cos_compinfo *dstci, capid_t dstcap, struct cos_compinfo *srcci, capid_t srccap);

int cos_thd_switch(thdcap_t c);
int cos_thd_wakeup(thdcap_t thd, tcap_t tc, tcap_prio_t prio, tcap_res_t res);
#define CAP_NULL 0
sched_tok_t cos_sched_sync(void);
/*
 * returns 0 on success and errno on failure:
 * -EBUSY: if rcv has pending notifications and if current thread is the thread associated with rcv.
 * -EAGAIN: if stok is outdated
 * -EPERM: if tcap is not active (has no budget left)
 * -EINVAL: any other error
 */
int cos_switch(thdcap_t c, tcap_t t, tcap_prio_t p, tcap_time_t r, arcvcap_t rcv, sched_tok_t stok);
int cos_thd_mod(struct cos_compinfo *ci, thdcap_t c, void *tls_addr); /* set tls addr of thd in captbl */

/*
 * returns 0 on success and errno on failure (the rcv thread will not be sent a notification):
 * -EBUSY: if rcv has pending notifications and if current thread is the thread associated with rcv.
 * -EAGAIN: if stok is outdated
 * -EPERM: if tcap is not active (has no budget left)
 * -EINVAL: any other error
 */
int cos_sched_asnd(asndcap_t snd, tcap_time_t timeout, arcvcap_t srcv, sched_tok_t stok);
/* returns 0 on success and -EINVAL on failure */
int cos_asnd(asndcap_t snd, int yield);
/* returns non-zero if there are still pending events (i.e. there have been pending snds) */
int cos_rcv(arcvcap_t rcv, rcv_flags_t flags, int *rcvd);
/* returns the same value as cos_rcv, but also information about scheduling events */
int cos_sched_rcv(arcvcap_t rcv, rcv_flags_t flags, tcap_time_t timeout, int *rcvd, thdid_t *thdid, int *blocked, cycles_t *cycles, tcap_time_t *thd_timeout);

int cos_introspect(struct cos_compinfo *ci, capid_t cap, unsigned long op);

vaddr_t cos_mem_alias(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src, unsigned long perm_flags);
vaddr_t cos_mem_aliasn(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src, size_t sz, unsigned long perm_flags);
vaddr_t cos_mem_aliasn_aligned(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src, size_t sz, size_t align, unsigned long perm_flags);
int     cos_mem_alias_at(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src, unsigned long perm_flags);
int     cos_mem_alias_atn(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src, size_t sz, unsigned long perm_flags);
vaddr_t cos_mem_move(struct cos_compinfo *dstci, struct cos_compinfo *srcci, vaddr_t src);
int     cos_mem_move_at(struct cos_compinfo *dstci, vaddr_t dst, struct cos_compinfo *srcci, vaddr_t src);
int     cos_mem_remove(pgtblcap_t pt, vaddr_t addr);

/* Tcap operations */
tcap_t cos_tcap_alloc(struct cos_compinfo *ci);
/*
 * returns 0 on success and errno on failure:
 * -EPERM: if src tcap is not active (has no budget left)
 * -EINVAL: any other error
 */
int cos_tcap_transfer(tcap_t src, arcvcap_t dst, tcap_res_t res, tcap_prio_t prio);
/*
 * returns 0 on success and errno on failure:
 * -EPERM: if src tcap is not active (has no budget left)
 * -EINVAL: any other error
 */
int cos_tcap_delegate(asndcap_t dst, tcap_t src, tcap_res_t res, tcap_prio_t prio, tcap_deleg_flags_t flags);
int cos_tcap_merge(tcap_t dst, tcap_t rm);

/* Hardware (interrupts) operations */
hwcap_t cos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap);
int     cos_hw_attach(hwcap_t hwc, hwid_t hwid, arcvcap_t rcvcap);
int     cos_hw_detach(hwcap_t hwc, hwid_t hwid);
void   *cos_hw_map(struct cos_compinfo *ci, hwcap_t hwc, paddr_t pa, unsigned int len);
int     cos_hw_cycles_per_usec(hwcap_t hwc);
int     cos_hw_cycles_thresh(hwcap_t hwc);
int     cos_hw_tlb_lockdown(hwcap_t hwc, unsigned long entryid, unsigned long vaddr, unsigned long paddr);
int     cos_hw_l1flush(hwcap_t hwc);
int     cos_hw_tlbflush(hwcap_t hwc);
int     cos_hw_tlbstall(hwcap_t hwc);
int     cos_hw_tlbstall_recount(hwcap_t hwc);
void    cos_hw_shutdown(hwcap_t hwc);


capid_t cos_capid_bump_alloc(struct cos_compinfo *ci, cap_t cap);

pgtblcap_t cos_shared_pgtbl_alloc(void);
u32_t cos_cons_into_shared_pgtbl(struct cos_compinfo *ci, pgtblcap_t top_lvl);

#endif /* COS_KERNEL_API_H */
