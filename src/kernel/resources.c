/***
 * ## Resource/page Abstractions
 *
 * Resources are page-based memory that is *typed* either into a
 * kernel-accessible data-structure, or as user-level virtual memory.
 * Resources are an implementation of user-level typing of kernel
 * memory pioneered by seL4, though the implementation and API are
 * very different and include many novel decisions. With this support,
 * the kernel has no dynamic memory allocation, instead relying on
 * user-level to use the API to piece together the corresponding
 * kernel data-structures. It is counter-intuitive that we can allow
 * user-level (untrusted) to in any way manage kernel memory (must be
 * trusted). This abstraction manages this by:
 *
 * 1. *Access control:* Only allowing components with capabilities to
 *    the resources to use these APIs.
 * 2. *Kernel integrity:* The kernel's integrity cannot be compromised
 *    using these APIs.
 * 3. *Liveness:* retyping is only allowed when (generally) no
 *    existing pointers remain, which places significant restrictions
 *    on when retyping can be performed.
 *
 * Lets take each of these in turn.
 *
 * **Access Control.** Resources are only available for
 * allocation/deallocation via retyping through capabilities in
 * last-level page-table nodes -- simplified here as "page-table
 * nodes". Page-table nodes are, themselves, resources. A component
 * must have a capability to a page-table node to perform retyping,
 * and must have the `COS_OP_CONSTRUCT | COS_OP_MODIFY_UPDATE`
 * permissions. Most components in the system should *not* have this
 * access, instead relying on management components to perform this
 * retyping for them. Once a page is typed into a kernel resource, it
 * is accessed as that resource through a capability in the
 * capability-table. Thus, to manage memory, a component requires a
 * page-table reference to the page, but to use the resource, a
 * component will do so through its capability-table.
 *
 * **Kernel Integrity.** The kernel maintains pointers between
 * resources, and relies on those resources having memory that is
 * properly formatted for its given type. Put another way, resources
 * must be well-typed in the kernel. When we're retyping a page into a
 * specific kernel resource (for example, a thread), we must pass in
 * all (capability-table) capabilities to the resources necessary to
 * properly conduct the retyping. For example, to create a thread, we
 * must pass the capability to the *scheduler thread* of the thread
 * we're trying to create, and a capability to the *component* we're
 * creating the thread in. Thus all of the type checks for the various
 * resource references can be performed when resources are retyped.
 * This ensures that the base-case of the resource introduced into the
 * kernel is safe. A key invariant is that all future operations
 * maintain a safe state. This is ensured by simply ensuring that
 * while there are references from other resources to the resource, it
 * cannot be retyped. User-level virtual memory cannot be reyped while
 * any user-level mappings exist. This ensures that the same page
 * cannot be treated as two separate types -- the main threat to
 * kernel integrity. For example, we cannot have the same page act as
 * a kernel thread (thus containing sensitive kernel state) and also
 * as user-level virtual memory (thus enabling user-level to corrupt
 * that state).
 *
 * **Liveness.** A core aspect of kernel integrity is ensuring that
 * there are no references to a resource when we're allowed to retype
 * it. A strict implementation of this is challenging. How can we tell
 * if there is a reference in a core's TLB to a user-level page?
 * Indeed how do we track if another core's using any of the
 * resources? Reference counting doesn't have the scalability
 * properties we require (see "Weak References" below), and cannot
 * track TLB references. We do maintain reference counts between
 * resources (a thread to its scheduler thread), but can't track the
 * rest of the references. Thus, Composite allows retyping to
 * *untyped* when all tracked references are removed, but untyped
 * memory cannot be used in any meaningful way. An untyped page is
 * only allowed to be retyped into user-level memory, or into a kernel
 * structure if the system has quiesced. Quiescence occurs when no
 * references are still possible. TLB quiescence, for example, occurs
 * when all TLBs have been flushed.
 */

#include "compiler.h"
#include "component.h"
#include "cos_error.h"
#include <atomics.h>
#include <chal_regs.h>

#include <resources.h>
#include <thread.h>
#include <captbl.h>
#include <pgtbl.h>
#include <compiler.h>

struct page_type page_types[COS_NUM_RETYPEABLE_PAGES] COS_PAGE_ALIGNED;
struct page      pages[COS_NUM_RETYPEABLE_PAGES];

/* The argument is really a page, but we want to keep it generic */
static inline pageref_t
page2ref(void *p)
{ return (struct page *)p - pages; }

static inline struct page_type *
page2type(void *p)
{ return &page_types[page2ref(p)]; }

__attribute__((noinline)) void
page_zero(struct page *p)
{
	unsigned int i;

	for (i = 0; i < (COS_PAGE_SIZE / COS_WORD_SIZE); i++) {
		p->words[i] = 0;
	}
}

static inline int
page_type_valid_active(page_type_t t, page_kerntype_t kt)
{
	if (t == COS_PAGE_TYPE_VM && kt == 0) return 1;
	if (t == COS_PAGE_TYPE_KERNEL && (kt >= COS_PAGE_KERNTYPE_THD && kt < COS_PAGE_KERNTYPE_NUM)) return 1;

	return 0;
}

/*
 * Is the page reference out of bounds? Returns `0` if it is in
 * bounds, and `1` if out of bounds.
 */
int
page_bounds_check(pageref_t ref)
{ return ref >= COS_NUM_RETYPEABLE_PAGES; }

/***
 * ### Resources and Retyping
 *
 * The kernel's *resources* are each sized to a page, and they each
 * have a *type* that defines the allowed functionality of that
 * resource. For example, resources typed as a "thread" can be
 * switched to by the scheduler, and the contents of that page include
 * the thread's register, id, etc... (see `struct thread`). So each
 * page is a *typed resource* that, at any point, has a *type* of:
 *
 * - Untyped - in which it cannot be used for anything directly, but
 *   can be *retyped* into one of the other types. If we want to
 *   retype between different kernel types or the VM type, we must go
 *   *through* the untyped type.
 * - Retyping - a *transient* state used while a resource is being
 *   updated.
 * - Virtual Memory (VM) - means that the page can be used for
 *   user-level mappings.
 * - Kernel - A kernel data-structure that is referenced from within
 *   kernel data-structures themselves. There are many of these kernel
 *   types, with separate `kerntype_t`s. These kernel types include
 *   threads, capability, and page-table nodes.
 *
 * To retype between the kernel of VM type *A* to type *B*, we must
 * retype from *A* to untyped, then to *B*. The alternative would be
 * for us to directly retype between different types directly, without
 * having to go through untyped each time. For example, from *A*
 * directly to *B*. Why is this? The untyped type exists to enable
 * synchronization.
 *
 * The parallel accesses allowed by the wait-free access to kernel
 * resources on any core require that we synchronize the paths in the
 * system for normal resource access, and the paths for
 * deallocation/allocation. We must prevent the case where a core is
 * accessing a resource, while another is zeroing, reinitializing, or
 * otherwise treating the memory as freed or re-allocated as
 * potentially a new type. Thus, when we retype into untyped, we don't
 * want to modify the resources memory so as to avoid tripping up
 * parallel accesses to the resource. Only when we retype from untyped
 * into another concrete type do we modify the memory. This must only
 * be allowed if *quiescence* has been achieved, which is to say that
 * all parallel accesses to the resource while it was still an active
 * resource type (before retyping into untyped) have completed.
 *
 * The conceptual states that each page goes through as part of the
 * quiescence logic follow:
 *
 * 1. resource begins typed as a kernel or virtual memory type (i.e.
 *    non-untyped/retyping),
 * 2. if there are no more references to the resource, retype it to
 *    untyped -- this prevents parallel computations from taking
 *    further references to the resource's memory,
 * 3. retype into retyping only if the resource has quiesced, thus
 *    there are no remaining references to the resource from the
 *    system (including parallel kernel system call accesses from
 *    other cores, page-table walker accesses, and cached TLB
 *    references to virtual memory pages). This zero's out the page
 *    which is valid due to the quiescence.
 * 4. initialize the resource as appropriate for its type.
 * 5. retype into the destination type as it is no longer referenced
 *    as the previous type, and is appropriately initialized as this
 *    type.
 *
 * And the cycle continues.
 *
 * **Summarized**: The state machine of these retyping operations has
 * the following states:
 *
 * - K - Kernel resource (i.e. component, thread, or resource table node)
 * - U - Untyped page
 * - V - Virtual memory
 *
 * Transitions include:
 *
 * - K -> U only if no references remain to K
 * - U -> V only if the page has quiesced, thus no parallel references exist, and no references remain
 * - V -> U only if no mappings/references remain to V
 * - U -> V only if the page has quiesced, thus no parallel references exist, and no references remain
 *
 * As such, you can see that the `untyped` state generally exists for
 * synchronization to enable quiescence.
 */

/**
 * `page_retype_from_untyped_reserve` attempts to begin the retyping
 * process of a resource from untyped into a specific other type
 * (kernel resource, or virtual memory). Will fail if the resource
 * hasn't quiesced (it is still "active", and *potentially* referenced
 * by logic on another core, or if it is not untyped.
 *
 * - `@t` - page type structure
 * - `@p` - corresponding page structure
 * - `@type` - desired type
 * - `@ktype`  - desired kernel type
 * - `@return` - normal error
 */
cos_retval_t
page_retype_from_untyped_reserve(struct page_type *t, struct page *p, page_type_t type, page_kerntype_t ktype)
{
	if (!page_type_valid_active(type, ktype)) return -COS_ERR_WRONG_INPUT_TYPE;
	if (t->type != COS_PAGE_TYPE_UNTYPED)     return -COS_ERR_WRONG_PAGE_TYPE;
	if (t->refcnt != 1)                       return -COS_ERR_STILL_REFERENCED;
	/* TODO: check quiescence */

	/* This ensures that we are the only core able to perform this retype */
	if (!cas8(&t->type, COS_PAGE_TYPE_UNTYPED, COS_PAGE_TYPE_RETYPING)) return -COS_ERR_WRONG_PAGE_TYPE;

	/*
	 * FIXME: this is not always required. Some previous types
	 * (restbl) will already be zeroed implicitly when they have
	 * the required refcnt. However, it is unclear how to teach
	 * the verification invariants that this is OK.
	 */
	/* Updates! */
	page_zero(p);
	t->kerntype = ktype;

 	return COS_RET_SUCCESS;
}

/**
 * `page_retype_from_untyped_commit` simply "commits" (as in data-base
 * transaction commit) the type into the resource to make it
 * accessible as that type.
 *
 * - `@t` - page type
 * - `@p` - corresponding page
 * - `@type` - desired page type
 */
void
page_retype_from_untyped_commit(struct page_type *t, struct page *p, page_type_t type)
{
	mem_barrier();
	/*
	 * Commit the updates. Now that the page's memory is
	 * initialized, make it accessible as the desired type.
	 */
	t->type = type;
}

/**
 * `page_retype_from_untyped` retypes a page that is untyped, into a
 * specific other type (either kernel, or user-level VM). Will fail if
 * the page isn't untyped, if it hasn't quiesced, or if another
 * parallel untype wins the race.
 *
 * - `@idx` - the reference to the resource to retype
 * - `@type` - the target type (not untyped/retyping)
 * - `@ktype` - the target kernel type
 * - `@return` - normal error
 */
cos_retval_t
page_retype_from_untyped(pageref_t idx, page_type_t type, page_kerntype_t ktype)
{
	struct page_type *t;
	struct page *p;

	ref2page(idx, &p, &t);
	COS_CHECK(page_retype_from_untyped_reserve(t, p, type, ktype));
	page_retype_from_untyped_commit(t, p, type);

	return COS_RET_SUCCESS;
}

/**
 * `page_retype_from_untyped` will attempt to move the resource from
 * its state as either user-level virtual memory or a kernel resource,
 * into untyped memory. This will fail if references remain to the
 * resource, or if it is already has the untyped (or retyping) type.
 *
 * - `@idx` - the reference to the resource to be retyped.
 * - `@return` - normal return values
 */
cos_retval_t
page_retype_to_untyped(pageref_t idx)
{
	struct page_type *t;
	page_type_t type;

	ref2page(idx, NULL, &t);
	/*
	 * Check that all conditions are satisfied for when can we
	 * move a page to untyped: we hold the last reference, and the
	 * type can be moved to untyped.
	 */
	if (t->refcnt != 0) return -COS_ERR_STILL_REFERENCED;
	type = t->type;
	/* If we have a valid type, convert it to "retyping" so that we "own" it. */
	if (type == COS_PAGE_TYPE_RETYPING || type == COS_PAGE_TYPE_UNTYPED) return -COS_ERR_WRONG_PAGE_TYPE;

	/*
	 * Lets synchronize to ensure that we have the ability to make
	 * updates.
	 */
	if (!cas8(&t->type, type, COS_PAGE_TYPE_RETYPING)) return -COS_ERR_WRONG_PAGE_TYPE;
	/* Invalidate existing pointers to the resource. */
//	faa(&t->epoch, 1);
	t->kerntype = 0;

	/* Commit the changes */
	t->type = COS_PAGE_TYPE_UNTYPED;

	return COS_RET_SUCCESS;
}

/**
 * `destroy_lookup_retype` looks up a resource reference in a
 * page-table node (located at a specific capability), with a
 * specific, expected type, and attempts to retype it to UNTYPED. This
 * can fail if the capability is wrong, the offset in the page-table
 * node is wrong, or if it cannot be retyped (e.g. because references
 * remain tot he resource).
 *
 * - `@ct` - the captbl to use for the lookup
 * - `@pgtbl_cap` - capability to use to find the page-table
 * - `@pgtbl_off` - the offset into the page-table node of the resource
 * - `@t` - expected type of the resource, and the
 * - `@kt` - expected kernel type
 * - `@pgref` - a return value reference to the resource
 * - `@return` - the normal return value
 */
cos_retval_t
destroy_lookup_retype(captbl_t ct, cos_cap_t pgtbl_cap, uword_t pgtbl_off, page_type_t t, page_kerntype_t kt, pageref_t *pgref)
{
	struct capability_resource *pgtbl;
	pageref_t pgtbl_ref;

	COS_CHECK(captbl_lookup_type_deref(ct, pgtbl_cap, COS_CAP_TYPE_PGTBL_LEAF, COS_OP_DEALLOCATE, &pgtbl_ref));
	/* TODO: pay attention to the permissions */
	COS_CHECK(pgtbl_leaf_lookup(pgtbl_ref, pgtbl_off, t, kt, COS_PGTBL_PERM_KERNEL, pgref));
	/* Note that this retype can only proceed after it checks the reference count */
	COS_CHECK(page_retype_to_untyped(*pgref));

	return COS_RET_SUCCESS;
}

/***
 * ### Weak References
 *
 * The *weak reference* API implements versioned pointers/references.
 * Reference counts are a conventional mechanism to prevent memory
 * from being deallocated (or in our case, retyped) while there are
 * pointers to that memory. This provides a restricted form of
 * garbage-collection, simplifying system memory management.
 * Unfortunately, reference counts can prevent scalable, parallel
 * system execution. The cache-line modifications necessary for atomic
 * increment/decrements prevent scalability. This is particularly a
 * problem in the thread migration path as we'd need to take a
 * reference of the server we're invoking, and release the reference
 * when returning. The cache-coherence involved would massively
 * increase the worst-case overhead of thread migration.
 *
 * The core idea behind weak references is that we can check if an
 * existing reference is to the intended resource. The reference will
 * outlive the resource, and we simply check the validity of the
 * reference before dereferencing it. To implement this, each resource
 * has an `epoch`, and each weak reference has a corresponding
 * `epoch`. Deallocating a resource (retyping it), increases the
 * epoch. Dereferencing a weak reference first checks that the epoch
 * of the reference matches that of the resource.
 *
 * The Composite kernel uses both weak references and reference
 * counters. All references from the capability table are weak
 * references, and all references from threads to components (on the
 * thread's invocation stack) are weak. This means that resources can
 * be retyped even with existing capability-table references. This is
 * important as it enables components in charge of resources to more
 * freely retype/deallocate which revokes resource access, regardless
 * how other components have delegated the resources. Reference counts
 * are still used to track the internal references in resource tables
 * (i.e. from a node at level N to a node at level N + 1), and thread
 * references for schedulers and tcaps.
 */

/* Get the page's epoch. */
static inline epoch_t
epoch_copy(struct page_type *pt)
{
	return load64(&pt->epoch);
}

/* Update the page's epoch, and return the previous value. */
static inline epoch_t
epoch_update(struct page_type *pt)
{
	return (epoch_t)faa(&pt->epoch, 1);
}

/**
 * `resource_weakref_create` creates a new weak reference to a
 * resource. Returns `COS_RET_SUCCESS` if the resource is a the
 * expected type, and populates the reference, `wr`. Otherwise return
 * a type error.
 *
 * A core challenge of this design is handling a race between on
 * dereference when checking both the type, and the epochs. We need to
 * retrieve the epoch *before* doing the type check as we have to
 * assume that the type is changed the *cycle* after we retrieve it.
 * Given that, the type of the page can immediately be changed, and
 * the (now) updated epoch will make this clear. We wish to avoid the
 * race:
 *
 * 1. check that the page is a component = success,
 * 2. another core retypes to the page to something else which
 *    includes updating its epoch,
 * 3. get its epoch, and place it into a reference.
 *
 * This creates a problem: the page has been updated to not be a
 * component, yet the epoch doesn't inform us (as it should) that the
 * component is no longer active.
 *
 * - `@resource_ref` - resource to which we want to create the reference
 * - `@expected_kerntype` - its expected type (assumes a kernel type)
 * - `@wr` - the weak reference to populate
 * - `@return` - normal return, can fail if the type doesn't match
 */
cos_retval_t
resource_weakref_create(pageref_t resource_ref, page_kerntype_t expected_kerntype, struct weak_ref *wr)
{
	struct page_type *t;
	struct page *p;
	epoch_t epoch;

	/* First take a snapshot of the epoch... */
	ref2page(resource_ref, NULL, &t);
	epoch = epoch_copy(t);
	mem_barrier();
	/* ...then typecheck the resource. */
	COS_CHECK(page_resolve(resource_ref, COS_PAGE_TYPE_KERNEL, expected_kerntype, NULL, NULL, NULL));

	*wr = (struct weak_ref) {
		.epoch = epoch,
		.ref = resource_ref,
	};

	return COS_RET_SUCCESS;
}

/**
 * `resource_compref_create` creates a new component reference
 * (including a weak reference) to the component. It returns
 * `COS_RET_SUCCESS` if the resource is a component, and populates the
 * component reference, `r`. Otherwise return a type error.
 *
 * - `@compref` - a reference to the component page.
 * - `@r` - a component reference to be filled out.
 * - `@return` - normal return value (success, or type failure)
 */
cos_retval_t
resource_compref_create(pageref_t compref, struct component_ref *r)
{
	struct page_type *t;
	struct component *comp;
	struct weak_ref wr;

	COS_CHECK(resource_weakref_create(compref, COS_PAGE_KERNTYPE_COMP, &wr));
	COS_CHECK(page_resolve(compref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_COMP, NULL, (struct page **)&comp, &t));

	*r = (struct component_ref) {
		.pgtbl = comp->pgtbl,
		.captbl = comp->captbl,
		.pd_tag = comp->pd_tag,
		.compref = wr,
	};

	return COS_RET_SUCCESS;
}

/**
 * `resource_comp_create`: An example of resource creation for a
 * component resource. This requires 1. addressing the capability
 * table entry to add, and 2. the page-table entry that points to your
 * untyped memory to use for the resource. Additionally, it takes
 * references to all of the resources that you need to create the
 * resource. In this case, this, includes the capability table,
 * page-table, protection domain, and entry instruction pointer for
 * the component.
 *
 * - `@captbl_ref` - the capability table to use for the component
 * - `@pgtbl_ref` - the page-table to use for the component
 * - `@pd` - the protection domain information to use for the component
 * - `@entry_ip` - the instruction pointer to use for thread creation
 *   in the component
 * - `@untyped_src_ref` - the untyped memory to use for the component
 * - `@return` - normal return value denoting error (negative values), or success (zero)
 */
cos_retval_t
resource_comp_create(captbl_ref_t captbl_ref, pgtbl_ref_t pgtbl_ref, prot_domain_tag_t pd, vaddr_t entry_ip, pageref_t untyped_src_ref)
{
	struct page_type *ptype, *pt_ptype, *ct_ptype;
	struct page *c_page;
	struct component *c;

	COS_CHECK(page_resolve(pgtbl_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_PGTBL_0, NULL, NULL, &pt_ptype));
	COS_CHECK(page_resolve(captbl_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_0, NULL, NULL, &ct_ptype));

	ref2page(untyped_src_ref, &c_page, &ptype);
	/* Retype the page, and if we don't unreserve the capability */
	COS_CHECK(page_retype_from_untyped_reserve(ptype, c_page, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_COMP));

	/* Take the references for the component's constituent captbl/pgtbl */
	faa(&pt_ptype->refcnt, 1);
	faa(&ct_ptype->refcnt, 1);

	c = (struct component *)c_page;
	/* Update the component structure in recently retyped page */
	*c = (struct component) {
		.pgtbl    = pgtbl_ref,
		.captbl   = captbl_ref,
		.pd_tag   = pd,
		.entry_ip = entry_ip,
		.fault_handler = { NULL }
	};
	/* Make the kernel resource accessible as a component */
	page_retype_from_untyped_commit(ptype, c_page, COS_PAGE_TYPE_KERNEL);

	/*
	 * Note that the reference count of the component page hasn't
	 * changed as we're both removing (a page-table reference) and
	 * adding a (capability-table) reference.
	 */

	return COS_RET_SUCCESS;
}

/**
 * `resource_comp_destroy` deactivates a component resource. In doing
 * so, it sheds its references to its capability and page-tables, and
 * increments its epoch, which atomically (modulo liveness
 * constraints) deactivates all references to that component (in
 * component capabilities, synchronous invocations, and thread
 * invocation stacks).
 *
 * - `@compref` - reference to the component page
 * - `@return` - normal return values (success by default, here)
 */
cos_retval_t
resource_comp_destroy(pageref_t compref)
{
	struct page_type *comp_ptype, *pt_ptype, *ct_ptype;
	struct component *c;

	ref2page(compref, (struct page **)&c, &comp_ptype);
	/* Make existing pointers in synchronous invocations be invalidated! */
	epoch_update(comp_ptype);
        /* These pointers are refcounted so chasing them can't fail */
	ref2page(c->pgtbl, NULL, &pt_ptype);
	ref2page(c->captbl, NULL, &ct_ptype);
	faa(&pt_ptype->refcnt, -1);
	faa(&ct_ptype->refcnt, -1);

	return COS_RET_SUCCESS;
}

/**
 * `resource_thd_create`: An example of resource creation for a
 * component resource. This requires 1. addressing the capability
 * table entry to add, and 2. the page-table entry that points to your
 * untyped memory to use for the resource. Additionally, it takes
 * references to all of the resources that you need to create the
 * resource. In this case, this, includes the capability table,
 * page-table, protection domain, and entry instruction pointer for
 * the component.
 *
 * - `@sched_thd_ref` - the scheduler thread
 * - `@tcap_thd_ref` - the thread to use as a tcap (can be `untyped_src_ref`)
 * - `@comp_ref` - the component reference in which to start the thread
 * - `@epoch` - the component's epoch in the component capability (possibly outdated)
 * - `@id` - the desired thread id of the thread
 * - `@entry_ip` - the instruction pointer to use for thread creation
 *   in the component
 * - `@sched_token` - the value that will be passed to the scheduler with events for this thread.
 * - `@untyped_src_ref` - the untyped memory to use for the component
 * - `@return` - normal return value denoting error (negative values), or success (zero)
 */
cos_retval_t
resource_thd_create(pageref_t sched_thd_ref, pageref_t tcap_thd_ref, pageref_t comp_ref, thdid_t id, vaddr_t entry_ip, id_token_t sched_token, pageref_t untyped_src_ref)
{
	struct page_type *ptype, *sched_ptype, *tcap_ptype, *comp_ptype;
	struct page *thd_page, *tcap_page;
	struct component_ref ref;
	struct thread *thd;
	int tcap_self = untyped_src_ref == tcap_thd_ref;

	COS_CHECK(page_resolve(sched_thd_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD, NULL, NULL, &sched_ptype));
	if (!tcap_self) COS_CHECK(page_resolve(tcap_thd_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD, NULL, NULL, &tcap_ptype));
	COS_CHECK(resource_compref_create(comp_ref, &ref));

	ref2page(untyped_src_ref, &thd_page, &ptype);
        /*
	 * Retype the page, and if we don't unreserve the capability.
         * Failure not possible after this point.
	 */
	COS_CHECK(page_retype_from_untyped_reserve(ptype, thd_page, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD));

        /*
	 * Take the references for the scheduler and tcap threads.
         * don't take a reference on the component, instead using
         * `epoch` to determine liveness using the component_ref.
	 */
	faa(&sched_ptype->refcnt, 1);
	if (!tcap_self) faa(&tcap_ptype->refcnt, 1);

	thd = (struct thread *)thd_page;
	/* Update the component structure in recently retyped page */
	*thd = (struct thread) {
		.id = id,
		.invstk = (struct invstk) {
			.head = 0, /* We're in the top entry */
			.entries = {
				(struct invstk_entry) { /* Top entry is the current component */
					.component = ref,
					.ip = 0,
					.sp = 0,
				},
			},
		},
		.sched_thd = sched_thd_ref,
		.tcap_thd = tcap_thd_ref,
		.regs = { 0 },
	};
	thd->regs.state = REG_STATE_SYSCALL;
	regs_prepare_upcall(&thd->regs, entry_ip, coreid(), id, 0);

	/* Make the kernel resource accessible as a thread */
	page_retype_from_untyped_commit(ptype, thd_page, COS_PAGE_TYPE_KERNEL);

	return COS_RET_SUCCESS;
}

cos_retval_t
resource_thd_destroy(pageref_t thdref)
{
	struct page_type *thd_ptype, *sched_ptype, *tcap_ptype, *comp_ptype;
	struct thread *t;

	COS_CHECK(page_resolve(thdref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD, NULL, (struct page **)&t, &thd_ptype));
        /* These pointers are refcounted so chasing them can't fail */
	ref2page(t->sched_thd, NULL, &sched_ptype);
	faa(&sched_ptype->refcnt, -1);
	if (t->tcap_thd != thdref) {
		ref2page(t->tcap_thd, NULL, &tcap_ptype);
		faa(&tcap_ptype->refcnt, -1);
	}

	return COS_RET_SUCCESS;
}

/**
 * `resource_restbl_create`: Create and initialize the page for a
 * resource table node. This includes all levels of capability-tables,
 * and page-tables.
 *
 * - `@kt` - the specific kernel type to create (must be a captbl/pgtbl type)
 * - `@untyped_src_ref` - the untyped memory to use for the component
 * - `@return` - normal return value denoting error (negative values), or success (zero)
 */
cos_retval_t
resource_restbl_create(page_kerntype_t kt, pageref_t untyped_src_ref)
{
	struct page_type *ptype;
	struct page *p;
	int i;

	ref2page(untyped_src_ref, &p, &ptype);
        /*
	 * Retype the page, and if we don't unreserve the capability.
         * Failure not possible after this point.
	 */
	COS_CHECK(page_retype_from_untyped_reserve(ptype, p, COS_PAGE_TYPE_KERNEL, kt));

	if (kt == COS_PAGE_KERNTYPE_CAPTBL_LEAF) {
		captbl_leaf_initialize((struct captbl_leaf *)p);
	} else if (page_is_captbl(kt)) { /* non-leaf captbl */
		captbl_intern_initialize((struct captbl_internal *)p);
	} else if (kt == COS_PAGE_KERNTYPE_PGTBL_0) { /* top-level page-table */
		pgtbl_top_initialize((struct pgtbl_top *)p);
	} else {		/* internal page-table node  */
		pgtbl_intern_initialize((struct pgtbl_internal *)p);
	}

	/* Make the kernel resource accessible as a thread */
	page_retype_from_untyped_commit(ptype, p, COS_PAGE_TYPE_KERNEL);

	return COS_RET_SUCCESS;
}
