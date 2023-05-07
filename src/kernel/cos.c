/**
 * What design options exist for the implementing the kernel's
 * data-structures, and tracking their liveness? To assess this, we'll
 * enumerate the pointers in the system (references between
 * structures), and list the options for their implementation.
 *
 * Page pointers in the system:
 *
 * 1. Captbl internal - captbl(lvl N)[i] -> captbl(lvl N + 1)
 * 2. Pgtbl internal - pa2va(pgtbl(lvl N)[i] & PGMASK) -> pgtbl(lvl N + 1)
 * 3. Pgtbl virtual memory - pa2va(pgtbl(maxlvl)[i] & PGMASK) -> virt page
 * 4. TLB virtual memory - TLB(pa2va(pgtbl(maxlvl)[i] & PGMASK)) -> virt page
 * 5. Captbl resource - captbl(maxlvl)[i] -> resource including:
 *
 *    - captbl(maxlvl)[i] -> captbl(lvl x)
 *    - captbl(maxlvl)[i] -> pgtbl(lvl x)
 *    - captbl(maxlvl)[i] -> thd
 *    - captbl(maxlvl)[i] -> ?CB
 *
 * 6. Tcap - thd.tcap -> thd | thd'
 * 7. Event thd - thd.event_thd -> thd'
 * 8. Component - component -> (captbl(lvl 0), pgtbl(lvl 0))
 * 9. Thd execution - thd.invstk[i] -> captbl(lvl 0) & pgtbl(lvl 0)
 * 10. Invocation component ref - component(captbl, pgtbl)
 *
 * The mechanisms for tracking liveness cluster as such:
 *
 * - Refcnt: Deallocation allowed only with no remaining references - has the overhead of reference counting
 * - Epoch: Deallocation despite remaining references - requires epoch checking on access
 * - Liveness: Deallocation requires awaiting completion of parallel computations - SMR quiescence
 * - HW-walk liveness: synchronizing with parallel page-table walks - page-table walker quiescence
 * - TLB: Hardware caches with references to pages - TLB quiescence
 *
 * The pointers can be implemented as such:
 *
 * 1. Captbl internal - (Refcnt or Epoch) and Liveness
 * 2. Pgtbl internal - Refcnt (can't change the hardware walker to check epochs), HW-walk liveness, and Liveness
 * 3. Pgtbl VM - Refcnt (same) and HW-walk, Liveness
 * 4. TLB VM - TLB (VM pages can be read into the TLB, thus require quiescence)
 * 5. Captbl resource -
 *
 *    - Captbl - (Refcnt or Epoch) and Liveness
 *    - Pgtbl - (Refcnt or Epoch), Liveness, and HW-walk liveness
 *    - Thd - Refcnt or Epoch -- Refcnt preferable, and liveness not required as threads are per-core
 *    - ?CB - Refcnt, Liveness (if not per-core as SCB), and TLB liveness -- References made from user-level, thus cannot control accesses and epoch liveness is not sufficient. Mapped into user-level thus, TLB quiescience required.
 *
 * 6-7. Tcap & Event thd - Refcnt (required for decision making, thus can't be invalidated, but per-core resources, thus no liveness requirements)
 * 8. Component - Options: 1. Inherit liveness from constituent captbl/pgtbl, or 2. allocate a page for it, and Refcnt or Epoch.
 * 9. Thd execution - (Refcnt or Epoch) and Liveness. If components inherit liveness from captbl/pgtbl, then if they use reference counting, refcnts must be maintained on invoke/return and if they use epochs, they must be checked for each resource. If components are separately tracked, then the same logic applies, but only for the single component resource. Alternatively, we can choose either the captbl OR the pgtbl to define the lifetime of the entire component.
 * 10.
 */

/**
 * There are many options for how to implement the system as a
 * co-design with verification. This is a rough "map" of iterative
 * implementations and optimizations along with their dependencies in
 * "[*]".
 *
 * Capability tables:
 *
 * - Single-level capability tables.
 * - 2-level captbls.
 * - Captbl internal node pointers as page offsets.
 * - Captbl internal node pointers as pointers.
 * - Nested capability tables.
 *
 * Liveness:
 *
 * - BKL.
 * - Parallel SMR-based liveness.
 * - Reference counts for everything, components inherit liveness of
 *   both resource tables. All pointers are valid if they exist.
 * - No invocations, thus a focus on separation.
 * - Invocations using thread migration. Component reference counts on
 *   invocation and return.
 * - Epochs for capability table internal nodes. Likely not an
 *   interesting design point as this isn't possible for page-tables.
 * - Centralized deactivation of components which implies epoch-based
 *   liveness. Necessary to control component tear-down with threads
 *   executing who-knows-where.
 * - TLB quiescence.
 * - Clustered reference counts for each page enable per-core
 *   reference count operations, but expensive detection of "no more
 *   reference" states.
 *
 * Temporal Capabilities:
 *
 * - Threads with explicit priority, programmed from scheduler. No
 *   TCaps. Single system scheduler.
 * - TCaps with one per event/scheduling thread. Two priorities.
 * - TCaps with budgets, and expended notifications.
 *
 * Scheduling:
 *
 * - Dispatch to a thread unconditionally. No multiple TCaps. No
 *   interrupts nor asynchronous activation.
 * - Dispatch with event detection and switching to event/scheduler
 *   thread.
 * - Events provided through the SCB.
 * - Do we even need explicitly addressed TCaps in dispatch?
 *
 * Inter-thread IPC:
 *
 * - Asynchronous activation w/ events & dispatch to event thread on
 *   activation and return.
 * - Asynchronous activation w/ events to event thread & dispatch to
 *   target thread & dispatch back to event thread on return.
 * - Asynchronous activation w/ events and appropriate dispatches to
 *   event thread & dispatch to target thread based on priorities &
 *   budget.
 * - Synchronous activation by 1. enabling server threads to await
 *   activation events, 2. receiving a brand along with an activation,
 *   and 3. auto-switching the client to server when dispatched. If a
 *   chain of these activations reaches a threshold, activate the
 *   event thread. Switch back to the client if no other client has
 *   activated the server. Otherwise, activate the event thread.
 *
 * Boss state:
 *
 * - Clustered reference counts per page. Expensive retyping, cheap
 *   alias.
 * - Refcounts on VM alias/rem, restbl cons/decons, thread cpy/rem.
 * - Epoch-based component reference/inlining and termination.
 * - SMR-based accesses to captbls and time-based accesses to pgtbls.
 * - Pointer-based captbl internal nodes, and direct walking.
 * - TLB quiescence on retype.
 * - Asynchronous thread activations & interrupts.
 * - Thread activations with dependencies & exceptions.
 */

/***
 * Implementation Layers:
 *
 * 1. *System call machinery*: convert between the ISA's conventions for
 *    system calls, into a struct-accessible set of registers, and C
 *    execution.
 * 2. *Capability resolution*: lookup all capabilities, and perform a
 *    permission check on the requested operations.
 * 3. *Resource operation logic*: for capability table and page-table
 *    manipulations (i.e. creating and removing resources), this uses
 *    the following layers:
 *    4a. *Page retyping*: to move pages between the various types,
 *        and constrain that movement.
 *    4b. *Capability activation/deactivation*: to safely add and
 *        remove capabilities that reference the resources.
 *    ...while control operations utilize one of the following layers:
 *    4c. *Thread operations*: Thread dispatch, asynchronous
 *        activation, suspension, and scheduling event queries.
 *    4d. *Synchronous rendezvous between threads*: Synchronous
 *        interactions between threads including exception handling.
 *    4e. *Invocations via thread migration*: fast-path coordination
 *        between components.
 */

#include <cos_types.h>
#include <types.h>
#include <cos_consts.h>
#include <chal_consts.h>
#include <chal_types.h>
#include <consts.h>

#include <resources.h>
#include <capabilities.h>
#include <component.h>

#include <cos_error.h>

struct globals_percore {
	struct regs registers;	/* must be the first item as we're going to use stack ops to populate these */
	struct thread *active_thread;
	uword_t invstk_head;
	captbl_t active_captbl;
} __attribute__((aligned(COS_COHERENCE_UNIT_SIZE)));

/*
 * Kernel global data-structures.
 */
struct globals_percore globals[COS_NUM_CPU];

COS_FORCE_INLINE static inline struct globals_percore *
state(void)
{
	return &globals[0];
}


static inline liveness_t
liveness_now()
{
	/* TODO: actual liveness. */
	return 0;
}

static inline int
liveness_quiesced(liveness_t past)
{
	/* TODO: actual liveness. */
	return liveness_now() > past;
}

/**
 * Address-space conversions. The address spaces include:
 *
 * - *physical addresses* (`paddr_t`) - The actual physical memory in
 *   the system, spanning from `0` to the size of DRAM in the system.
 *   These address cannot be dereferenced, but they are used in two
 *   key places: 1. page-tables (including `%cr3`) as they map virtual
 *   to physical addresses, thus their references themselves must
 *   avoid virtual addresses, and 2. in device drivers, DMA references
 *   are physical addresses as the devices (without an I/O-MMU) only
 *   understand physical memory.
 *
 * - *kernel virtual addresses* (`void *`) - These are the "normal"
 *   addresses that are valid within the kernel. They are at
 *   high-addresses in each virtual address space.
 *
 * - *page references* (`pageref_t`) - The offset of the page being
 *   referenced into our typed page array. These can bit-wise be
 *   smaller than normal addresses, so they can be quite useful to
 *   compress the size of some structures. These references are also
 *   used to index into the page type metadata, thus are essential in
 *   many parts of the system. Given this, the conversion functions
 *   also (optionally) return the page type. As there are only
 *   `COS_NUM_RETYPEABLE_PAGES` pages, the page references are bounds
 *   checked.
 *
 * - *user-level virtual addresses* (`vaddr_t`) - An address usable by
 *   user-level component code. This is *not* dereference-able within
 *   the kernel, so you should never cast away from this type into a
 *   pointer. This is mainly used when setting the starting execution
 *   instruction pointer, or the synchronous invocation entry points.
 *   The only way to convert these is through page-tables, and should
 *   not be done by kernel code. Given this, there are no conversion
 *   functions below for this namespace.
 */

/* The argument is really a page, but we want to keep it generic */
static inline paddr_t
page2phys(void *va)
{ return (paddr_t)va; }

/* return the generic type to enable call-site typing */
static inline void *
phys2page(paddr_t pa)
{ return (struct page *)pa; }


static inline int
component_is_alive(struct component_ref *comp)
{
	struct page_type *t;

	ref2page(comp->compref, NULL, &t);

	return t->epoch == comp->epoch;
}

static inline cos_retval_t
component_activate(struct component_ref *comp)
{
	if (unlikely(!component_is_alive(comp))) return -COS_ERR_NOT_LIVE;

	/* Load page-table, save captbl globally. */

	return;
}

static inline pgtbl_t
pgtbl_arch_entry_pack(pageref_t ref, uword_t perm)
{
	return (ref << 12) & perm;
}

static inline void
pgtbl_arch_entry_unpack(pgtbl_t entry, pageref_t *ref, uword_t *perm)
{
	if (ref  != NULL) *ref  = entry >> 12;
	if (perm != NULL) *perm = entry & ((1 << 12) - 1);
}

static inline int
pgtbl_arch_entry_empty(pgtbl_t entry)
{
	return entry == 0;
}

struct pgtbl_top {
	pgtbl_t next[COS_PGTBL_TOP_NENT];
	pgtbl_t kern_next[COS_PGTBL_KERN_NENT];
};

struct pgtbl_internal {
	pgtbl_t next[COS_PGTBL_INTERNAL_NENT];
};

static inline int
page_is_pgtbl(page_kerntype_t type)
{ return !(type < COS_PAGE_KERNTYPE_PGTBL_0 || type >= (COS_PAGE_KERNTYPE_PGTBL_0 + COS_PGTBL_MAX_DEPTH)); }

void
pgtbl_top_initialize(struct pgtbl_top *pt)
{
	int i;

	/* The top-level of a page-table must include the kernel mappings. */
	for (i = 0; i < COS_PGTBL_TOP_NENT; i++) {
		pt->next[i] = 0;
	}
	for (i = 0; i < COS_PGTBL_KERN_NENT; i++) {
		pt->kern_next[i] = 0; /* TODO: copy kern mappings */
	}
}

void
pgtbl_intern_initialize(struct pgtbl_internal *pt)
{
	/*
	 * we have an internal page-table. Zeroing it out
	 * should yield entries with vacuous permissions.
	 */
	page_zero((struct page *)pt);
}

static cos_retval_t
pgtbl_construct(pgtbl_ref_t top, uword_t offset, pgtbl_ref_t bottom, uword_t perm)
{
	struct pgtbl_internal  *top_node, *bottom_node;
	struct page_type       *top_type, *bottom_type;
	page_kerntype_t         ktype;
	uword_t                 bound;

	if (page_bounds_check(top) || page_bounds_check(bottom)) return -COS_ERR_OUT_OF_BOUNDS;
	ref2page(top, (struct page **)&top_node, &top_type);
	if (!top_node) return -COS_ERR_OUT_OF_BOUNDS;

	ktype = top_type->kerntype;
	if (!page_is_pgtbl(ktype)) return -COS_ERR_WRONG_INPUT_TYPE;

	bound = ktype == COS_PAGE_KERNTYPE_PGTBL_0 ? COS_PGTBL_TOP_NENT : COS_PGTBL_INTERNAL_NENT;
	offset = COS_WRAP(offset, bound);

	COS_CHECK(page_resolve(bottom, COS_PAGE_TYPE_KERNEL, ktype + 1, NULL, (struct page **)&bottom_node, &bottom_type));

	if (ktype == COS_PAGE_KERNTYPE_PGTBL_0) {
		struct pgtbl_top *top_node;

		COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, ktype, NULL, (struct page **)&top_node, &top_type));

		if (top_node->next[offset]) return -COS_ERR_ALREADY_EXISTS;

		/* Updates! */
		if (!cas64(&top_node->next[offset], 0, page2phys(bottom_node) | (perm & COS_PGTBL_ALLOWED_INTERN_PERM))) return -COS_ERR_ALREADY_EXISTS;
	} else {
		struct pgtbl_internal *top_node;

		COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, ktype, NULL, (struct page **)&top_node, &top_type));

		if (top_node->next[offset]) return -COS_ERR_ALREADY_EXISTS;

		/* Updates! */
		if (!cas64(&top_node->next[offset], 0, page2phys(bottom_node) | (perm & COS_PGTBL_ALLOWED_INTERN_PERM))) return -COS_ERR_ALREADY_EXISTS;
	}

	faa(&top_type->refcnt, 1);
	faa(&bottom_type->refcnt, 1);

	return COS_RET_SUCCESS;
}

/* TODO: remove bottom argument? */
static cos_retval_t
pgtbl_deconstruct(pgtbl_ref_t top, pgtbl_ref_t bottom, uword_t offset)
{
	struct pgtbl_internal  *top_node, *bottom_node;
	struct page_type       *top_type, *bottom_type;
	page_kerntype_t         ktype;
	uword_t                 bound;
	pgtbl_t                 entry;

	if (page_bounds_check(top) || page_bounds_check(bottom)) return -COS_ERR_OUT_OF_BOUNDS;
	ref2page(top, (struct page **)&top_node, &top_type);

	ktype = top_type->kerntype;
	if (!page_is_pgtbl(ktype)) return -COS_ERR_WRONG_INPUT_TYPE;

	bound = ktype == COS_PAGE_KERNTYPE_PGTBL_0 ? COS_PGTBL_TOP_NENT : COS_PGTBL_INTERNAL_NENT;
	offset = COS_WRAP(offset, bound);

	COS_CHECK(page_resolve(bottom, COS_PAGE_TYPE_KERNEL, ktype + 1, NULL, (struct page **)&bottom_node, &bottom_type));

	entry = top_node->next[offset];
	if ((entry & ~COS_PGTBL_PERM_MASK) != page2phys(bottom_node)) return -COS_ERR_NO_MATCH;

	/* updates! */
	if (!cas64(&top_node->next[offset], entry, 0)) return -COS_ERR_NO_MATCH;
	faa(&top_type->refcnt, -1);
	faa(&bottom_type->refcnt, -1);

	return COS_RET_SUCCESS;
}

/**
 * `pgtbl_map` takes a page-table node reference, `pt`, (that
 * references the last-level of the page-table), an `offset` into that
 * node to find a specific entry, and attempts to map a given `page`
 * reference with the given permissions (`perm`). Note that the
 * `offset` is wrapped to be a valid index into the page-table node,
 * rather than bounds-checked. Returns typical return values.
 */
cos_retval_t
pgtbl_map(pgtbl_ref_t pt, uword_t offset, pageref_t page, uword_t perm)
{
	struct pgtbl_internal  *pt_node;
	struct page            *mem_node;
	struct page_type       *pt_type, *mem_type;

	if (page_bounds_check(pt)) return -COS_ERR_OUT_OF_BOUNDS;
	offset = COS_WRAP(offset, COS_PGTBL_INTERNAL_NENT);

        ref2page(pt, (struct page **)&pt_node, &pt_type);
	if (pt_type->type != COS_PAGE_TYPE_KERNEL ||
	    pt_type->kerntype != (COS_PAGE_KERNTYPE_PGTBL_0 + (COS_PGTBL_MAX_DEPTH - 1))) return -COS_ERR_WRONG_PAGE_TYPE;
	if (!pgtbl_arch_entry_empty(pt_node->next[offset])) return -COS_ERR_ALREADY_EXISTS;

        COS_CHECK(page_resolve(page, COS_PAGE_TYPE_VM, 0, NULL, &mem_node, &mem_type));

	/* Updates! */
	if (!cas64(&pt_node->next[offset], 0, pgtbl_arch_entry_pack(page, perm))) return -COS_ERR_ALREADY_EXISTS;
	faa(&pt_type->refcnt, 1);
	faa(&mem_type->refcnt, 1);

	return COS_RET_SUCCESS;
}

cos_retval_t
pgtbl_unmap(pgtbl_ref_t pt, uword_t offset)
{
	struct pgtbl_internal  *pt_node;
	struct page            *mem_node;
	struct page_type       *pt_type, *mem_type;
	pageref_t               page;
	pgtbl_t                 entry;

	if (page_bounds_check(pt)) return -COS_ERR_OUT_OF_BOUNDS;
	offset = COS_WRAP(offset, COS_PGTBL_INTERNAL_NENT);

	ref2page(pt, (struct page **)&pt_node, &pt_type);
	if (pt_type->type != COS_PAGE_TYPE_KERNEL ||
	    pt_type->kerntype != (COS_PAGE_KERNTYPE_PGTBL_0 + (COS_PGTBL_MAX_DEPTH - 1))) return -COS_ERR_WRONG_PAGE_TYPE;

	entry = pt_node->next[offset];
	if (pgtbl_arch_entry_empty(entry)) return -COS_ERR_RESOURCE_NOT_FOUND;

        pgtbl_arch_entry_unpack(entry, &page, NULL); /* TODO: permissions */
        COS_CHECK(page_resolve(page, COS_PAGE_TYPE_VM, 0, NULL, &mem_node, &mem_type));
	/* TODO: if the target page is untyped (or maybe a kernel type), we shouldn't unmap if this is the last ref. */

	/* Updates! */
	if (!cas64(&pt_node->next[offset], entry, 0)) return -COS_ERR_RESOURCE_NOT_FOUND;
	faa(&pt_type->refcnt, -1);
	faa(&mem_type->refcnt, -1);

	return COS_RET_SUCCESS;
}

/**
 * `pgtbl_leaf_lookup` does a lookup into a leaf-level node of the
 * page-table, and returns a reference to the associated reference, or
 * error if it doesn't have the expected type (or is a NULL
 * reference).
 *
 * - `@pgtbl_ref` - reference to the page-table leaf node in which to do the lookup
 * - `@pgtbl_src_off` - offset into that page-table node
 * - `@expected_type` - the expected resource type, and
 * - `@expected_kerntype` - the expected kernel type
 * - `@required_perm` - permissions that are required for the access
 * - `@resource_ref` - the returned reference to the corresponding resource
 * - `@return` - success or errors based on incorrect offset, permissions, or type
 */
static inline cos_retval_t
pgtbl_leaf_lookup(pgtbl_ref_t pgtbl_ref, uword_t pgtbl_src_off, page_type_t expected_type, page_kerntype_t expected_kerntype, uword_t required_perm, pageref_t *resource_ref)
{
	struct pgtbl_internal *pgtbl_entry;
	uword_t perm;

        /* Lookup the page to use as the backing memory for the component */
	pgtbl_entry = (struct pgtbl_internal *)ref2page_ptr(pgtbl_ref);
	/* TODO: use the permission bits */
	pgtbl_arch_entry_unpack(pgtbl_entry->next[COS_WRAP(pgtbl_src_off, COS_PGTBL_INTERNAL_NENT)], resource_ref, &perm);
	if (required_perm != 0 && (required_perm & perm) != required_perm) return -COS_ERR_INSUFFICIENT_PERMISSIONS;
	COS_CHECK(page_resolve(*resource_ref, expected_type, expected_kerntype, NULL, NULL, NULL));

	return COS_RET_SUCCESS;
}

/**
 * `comp_create`: The system call entry point for the
 * component creation API. A component requires 1. memory to back the
 * component (via `pgtbl_src_cap` and `pgtbl_src_off`), 2. the
 * references to the resources required for the component (captbl and
 * pgtbl), and 3. other necessary information such as the instruction
 * pointer with which threads should start execution in the component,
 * and protection domain information.
 *
 * - `@ct` - the current capability table
 * - `@comp_ct_cap` - the capability to the capability table leaf entry
 *   to use to store the capability to the new component.
 * - `@captbl_comp_off` - the offset into the `comp_ct_cap` entry to
 *   identify the new capability.
 * - `@captbl_cap` - Capability to the captbl for the component.
 * - `@pgtbl_cap` - Capability to the pgtbl for the component.
 * - `@ops` - operations allowed for the component.
 * - `@pd` - protection domain information for the component.
 * - `@entry_ip` - The entry instruction pointer for threads in the
 *   component.
 * - `@pgtbl_src_cap` - The capability for the page-table entry that
 *   holds the untyped memory to use to construct the component.
 * - `@pgtbl_src_off` - The offset into `pgtbl_src_cap` to the memory.
 * - `@return` - `COS_RET_SUCCESS` or a negative error value.
 */
cos_retval_t
comp_create(captbl_t ct, cos_cap_t captbl_comp_cap, uword_t captbl_comp_off, cos_cap_t captbl_cap, cos_cap_t pgtbl_cap,
	    cos_op_bitmap_t ops, prot_domain_tag_t pd, vaddr_t entry_ip, cos_cap_t pgtbl_src_cap, uword_t pgtbl_src_off)
{
	struct capability_resource *comp_captbl, *captbl, *pgtbl, *untyped_pgtbl;
	pageref_t untyped_src_ref;


	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, captbl_comp_cap, COS_CAP_TYPE_CAPTBL_LEAF, COS_OP_MODIFY_ADD, comp_captbl));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, pgtbl_cap,       COS_CAP_TYPE_PGTBL_0,     COS_OP_CONSTRUCT,  pgtbl));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, captbl_cap,      COS_CAP_TYPE_CAPTBL_0,    COS_OP_CONSTRUCT,  captbl));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, pgtbl_src_cap,   COS_CAP_TYPE_PGTBL_LEAF,  COS_OP_CONSTRUCT | COS_OP_MODIFY_UPDATE, untyped_pgtbl));
	COS_CHECK(pgtbl_leaf_lookup(untyped_pgtbl->intern.ref, pgtbl_src_off, COS_PAGE_TYPE_UNTYPED, 0, 0, &untyped_src_ref));

	return cap_create_comp(comp_captbl->intern.ref, captbl_comp_off, ops, captbl->intern.ref, pgtbl->intern.ref, pd, entry_ip, untyped_src_ref);
}

/**
 * `comp_destroy` takes the capability table (`ct`), the capability to
 * a last-level page-table node that includes a component resource
 * (`pgtbl_cap`), and the offset into that page-table node of the
 * target component (`pgtbl_off`).
 *
 * - `@ct` - capability table to lookup into
 * - `@pgtbl_cap` - capability to the page-table node with the comp ref
 * - `@pgtbl_off` - offset into the page-table node to the component
 * - `@return` - normal success/error return value.
 */
cos_retval_t
comp_destroy(captbl_t ct, cos_cap_t pgtbl_cap, uword_t pgtbl_off)
{
	pageref_t compref;

	COS_CHECK(destroy_lookup_retype(ct, pgtbl_cap, pgtbl_off, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_COMP, &compref));
        /*
         * At this point, we're committed as we've updated the page
         * type to `COS_PAGE_TYPE_RETYPING`. Remove references to the
         * resource tables, and update the component's epoch to
         * invalidate. None of this should be able to fail, so we
         * don't need to undo the retype that was part of
         * `page_retype_to_untyped`.
         *
	 * Commence the updates to teardown the component page.
	 */
	COS_CHECK(resource_comp_destroy(compref));

	return COS_RET_SUCCESS;
}


/**
 * `sinv_create` creates a synchronous invocation to a given
 * component.
 *
 * - `@ct` - capability table to use for lookups
 * - `@captbl_sinv_cap` - the capability to the captbl to add the sinv
 * - `@captbl_sinv_off` - the offset into that captbl node
 * - `@comp_cap` - the component capability to which the sinv refs
 * - `@entry_ip` - entry instruction pointer into component
 * - `@token` - the token to pass to the component on invocation
 * - `@return` - Normal return value.
 */
cos_retval_t
sinv_create(captbl_t ct, cos_cap_t captbl_sinv_cap, uword_t captbl_sinv_off, cos_cap_t comp_cap, vaddr_t entry_ip, inv_token_t token)
{
	struct capability_resource *sinv_captbl, *comp;

	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, captbl_sinv_cap, COS_CAP_TYPE_CAPTBL_LEAF, COS_OP_MODIFY_ADD, sinv_captbl));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, comp_cap,        COS_CAP_TYPE_COMP,        COS_OP_CONSTRUCT,  comp));

	return cap_create_sinv(sinv_captbl->intern.ref, captbl_sinv_off, comp->intern.ref, entry_ip, token);
}

/**
 * `thd_create`: The system call entry point for the thread
 * creation API. A thread requires 1. memory to back the thread (via
 * `pgtbl_src_cap` and `pgtbl_src_off`), 2. the references to the
 * resources required for the thread (the component in which to create
 * the thread, the scheduler and tcap threads), and 3. other necessary
 * information such as the thread id and the token to pass to the
 * scheduling thread with this thread's events.
 *
 * - `@ct` - the current capability table
 * - `@comp_ct_cap` - the capability to the capability table leaf entry
 *   to use to store the capability to the new component.
 * - `@captbl_comp_off` - the offset into the `comp_ct_cap` entry to
 *   identify the new capability.
 * - `@captbl_cap` - Capability to the captbl for the component.
 * - `@pgtbl_cap` - Capability to the pgtbl for the component.
 * - `@ops` - operations allowed for the component.
 * - `@pd` - protection domain information for the component.
 * - `@entry_ip` - The entry instruction pointer for threads in the
 *   component.
 * - `@pgtbl_src_cap` - The capability for the page-table entry that
 *   holds the untyped memory to use to construct the component.
 * - `@pgtbl_src_off` - The offset into `pgtbl_src_cap` to the memory.
 * - `@return` - `COS_RET_SUCCESS` or a negative error value.
 */
cos_retval_t
thd_create(captbl_t ct, cos_cap_t captbl_comp_cap, uword_t captbl_comp_off, cos_cap_t schedthd_cap, cos_cap_t tcapthd_cap, cos_cap_t comp_cap,
	   cos_op_bitmap_t ops, thdid_t id, id_token_t token, cos_cap_t pgtbl_src_cap, uword_t pgtbl_src_off)
{
	struct capability_resource *comp_captbl, *untyped_pgtbl;
	struct capability_resource *sched_thd, *tcap_thd;
	struct capability_component *comp;
	struct component *c;
	pageref_t untyped_src_ref;

	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, captbl_comp_cap, COS_CAP_TYPE_CAPTBL_LEAF, COS_OP_MODIFY_ADD, comp_captbl));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, schedthd_cap,    COS_CAP_TYPE_THD,         COS_OP_CONSTRUCT,  sched_thd));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, tcapthd_cap,     COS_CAP_TYPE_THD,         COS_OP_CONSTRUCT,  tcap_thd));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, comp_cap,        COS_CAP_TYPE_COMP,        COS_OP_CONSTRUCT,  comp));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, pgtbl_src_cap,   COS_CAP_TYPE_PGTBL_LEAF,  COS_OP_CONSTRUCT | COS_OP_MODIFY_UPDATE, untyped_pgtbl));
	COS_CHECK(pgtbl_leaf_lookup(untyped_pgtbl->intern.ref, pgtbl_src_off, COS_PAGE_TYPE_UNTYPED, 0, 0, &untyped_src_ref));

	return cap_create_thd(comp_captbl->intern.ref, captbl_comp_off, ops, sched_thd->intern.ref, tcap_thd->intern.ref, comp->intern.comp, comp->intern.epoch, id, token, untyped_src_ref);
}

/**
 * `thd_destroy` takes the capability table (`ct`), the capability to
 * a last-level page-table node that includes a thread resource
 * (`pgtbl_cap`), and the offset into that page-table node of the
 * target component (`pgtbl_off`).
 *
 * Returns the normal error/success.
 */
cos_retval_t
thd_destroy(captbl_t ct, cos_cap_t pgtbl_cap, uword_t pgtbl_off)
{
	pageref_t thdref;

	COS_CHECK(destroy_lookup_retype(ct, pgtbl_cap, pgtbl_off, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD, &thdref));
        /*
         * At this point, we're committed as we've updated the page
         * type to `COS_PAGE_TYPE_RETYPING`. Remove references to the
         * resource tables. None of this should be able to fail, so we
         * don't need to undo the retype that was part of
         * `page_retype_to_untyped`.
         *
	 * Commence the updates to teardown the thread page.
	 */
	COS_CHECK(resource_thd_destroy(thdref));

	return COS_RET_SUCCESS;
}

/**
 * `restbl_create`: The system call entry point for the thread
 * creation API. A thread requires 1. memory to back the thread (via
 * `pgtbl_src_cap` and `pgtbl_src_off`), 2. the references to the
 * resources required for the thread (the component in which to create
 * the thread, the scheduler and tcap threads), and 3. other necessary
 * information such as the thread id and the token to pass to the
 * scheduling thread with this thread's events.
 *
 * - `@ct` - the current capability table
 * - `@comp_restbl_cap` - the capability to the capability table leaf entry
 *   to use to store the capability to the new component.
 * - `@captbl_restbl_off` - the offset into the `comp_ct_cap` entry to
 *   identify the new capability.
 * - `@pgtbl_src_cap` - The capability for the page-table entry that
 *   holds the untyped memory to use to construct the component.
 * - `@pgtbl_src_off` - The offset into `pgtbl_src_cap` to the memory.
 * - `@return` - `COS_RET_SUCCESS` or a negative error value.
 */
cos_retval_t
restbl_create(captbl_t ct, cos_cap_t captbl_restbl_cap, uword_t captbl_restbl_off, page_kerntype_t kt, cos_op_bitmap_t ops, cos_cap_t pgtbl_src_cap, uword_t pgtbl_src_off)
{
	struct capability_resource *restbl_captbl, *untyped_pgtbl;
	struct capability_component *comp;
	struct component *c;
	pageref_t untyped_src_ref;

	if (!page_is_pgtbl(kt) && !page_is_captbl(kt)) return -COS_ERR_WRONG_INPUT_TYPE;
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, captbl_restbl_cap, COS_CAP_TYPE_CAPTBL_LEAF, COS_OP_MODIFY_ADD, restbl_captbl));
	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, pgtbl_src_cap,   COS_CAP_TYPE_PGTBL_LEAF,  COS_OP_CONSTRUCT | COS_OP_MODIFY_UPDATE, untyped_pgtbl));
	COS_CHECK(pgtbl_leaf_lookup(untyped_pgtbl->intern.ref, pgtbl_src_off, COS_PAGE_TYPE_UNTYPED, 0, 0, &untyped_src_ref));

	return cap_create_restbl(restbl_captbl->intern.ref, captbl_restbl_off, kt, ops, untyped_src_ref);
}

/**
 * `restbl_destroy` takes the capability table (`ct`), the capability
 * to a last-level page-table node that includes a thread resource
 * (`pgtbl_cap`), and the offset into that page-table node of the
 * target component (`pgtbl_off`). Also takes the kernel type of the
 * node that must be one of the capability- or page-table node types.
 * Deallocates/retypes the resource table node.
 *
 * Returns the normal error/success.
 */
cos_retval_t
restbl_destroy(captbl_t ct, cos_cap_t pgtbl_cap, uword_t pgtbl_off, page_kerntype_t kt)
{
	pageref_t rtref;

	COS_CHECK(destroy_lookup_retype(ct, pgtbl_cap, pgtbl_off, COS_PAGE_TYPE_KERNEL, kt, &rtref));
        /*
	 * If the resource table's refcnt allows deallocation, then
         * there are no links we need to destroy. We'll just clean up
         * the memory on retype! In other words, nothing more to do here.
	 */

	return COS_RET_SUCCESS;
}




COS_NEVER_INLINE struct regs *
capability_activation_slowpath(struct regs *rs, struct capability_generic *cap)
{
	struct globals_percore *g = state();
	captbl_t captbl = g->active_captbl;
	struct thread *t = g->active_thread;

        /*  */
	if (capability_is_captbl(cap)) {
		printk("captbl processing\n");
	} else if (capability_is_pgtbl(cap)) {
		printk("pgtbl processing\n");
	} else if (cap->type == COS_CAP_TYPE_HW) {
		printk("hw processing\n");
	} else {
		printk("otherwise processing\n");
	}

        return rs;
}

COS_FORCE_INLINE static inline coreid_t
coreid(void)
{
	return 0;
}

COS_FASTPATH static inline struct regs *
capability_activation(struct regs *rs)
{
	struct globals_percore *g = state();
	captbl_t captbl = g->active_captbl;
	struct thread *t = g->active_thread;
	struct capability_generic *cap_slot;
	cos_cap_t cap = regs_arg(rs, REGS_ARG_CAP);
	cos_op_bitmap_t ops;

        /*
	 * The synchronous invocation fastpath includes invoke and
         * return. Capability #0 is hard-coded to synchronous return
         * fastpath, while an invocation is signaled by finding a
         * synchronous invocation capability.
	 */
	if (likely(cap == 0)) {
		return sinv_return(t, &g->invstk_head, rs);
	}
	cap_slot = captbl_lookup(captbl, cap);
	if (likely(cap_slot->type == COS_CAP_TYPE_SINV)) {
		struct capability_sync_inv *sinv_cap = (struct capability_sync_inv *)cap_slot;
		/*
		 * Synchronous invocation capabilities only provide a
		 * single operation -- component function invocation,
		 * so the capability alone is sufficient to kick off
		 * the invocation.
		 */
		return sinv_invoke(t, &g->invstk_head, rs, sinv_cap);
	}

	ops = regs_arg(rs, REGS_ARG_OPS);
        /*
	 * Validate that all of the requested operations are allowed.
         * We didn't need to do this before the synchronous invocation
         * operations, as there is only a single operation
         * (invocation) allowed on those capabilities.
	 */
	if (unlikely((ops & cap_slot->operations) != ops)) {
		/* TODO: software exception */
		regs_retval(rs, REGS_RETVAL_BASE, -COS_ERR_INSUFFICIENT_PERMISSIONS);

                return rs;
	}

        /*
	 * Thread operations are both performance sensitive (IPC), and
         * complex. These are the only operations that switch threads,
         * thus all of the global register update logic is here.
	 */
	if (likely(cap_slot->type == COS_CAP_TYPE_THD)) {
		struct capability_resource *cap_thd = (struct capability_resource *)cap_slot;
		struct thread *t = (struct thread *)ref2page_ptr(cap_thd->intern.ref);

		/* Thread operations. First, the dispatch fast-path. */
		if (likely(ops == COS_OP_THD_DISPATCH)) {
                        return thread_switch(t, rs, 0);
		}

		return thread_slowpath(t, ops, rs);
	}

	return capability_activation_slowpath(rs, cap_slot);
}

__attribute__((section(".ipc_fastpath")))
reg_state_t
syscall_handler(struct regs **rs)
{
	struct regs *ret_regs = capability_activation(*rs);

        /*
	 * Here we return a pointer to the *top* of the registers to
         * enable restores via a sequence of pop instructions.
	 */
        *rs = &ret_regs[1];

	return (*rs)->state
}
