#include <atomics.h>
#include <chal_regs.h>

#include <resources.h>
#include <thread.h>
#include <captbl.h>

struct page_type       page_types[COS_NUM_RETYPEABLE_PAGES] __attribute__((aligned(COS_PAGE_SIZE)));
struct page            pages[COS_NUM_RETYPEABLE_PAGES];


/* Get the page's epoch. */
static inline epoch_t
epoch_copy(struct page_type *pt)
{
	return pt->epoch;
}

/* Update the page's epoch, and return the previous value. */
static inline epoch_t
epoch_update(struct page_type *pt)
{
	return (epoch_t)faa(&pt->epoch, 1);
}

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

/**
 * `ref2page` finds a pointer to a page from its reference. Returns
 * the generic type to enable call-site typing.
 *
 * Assumes: `ref` is a in-bound reference to a page. No bounds
 * checking is done here. This is only reasonable given verification
 * that can assert that all refs are in-bound. Additionally, return
 * the type and metadata information for the page.
 *
 * - `@ref` - the resource reference
 * - `@p` - the returned page structure
 * - `@t` - the returned page_type structure
 */
void
ref2page(pageref_t ref, struct page **p, struct page_type **t)
{
	/*
	 * Page references should be implementation-internal. Use
	 * verification invariants to guarantee they are valid
	 * values.
	 */
//	if (unlikely(ref_bounds_check(ref))) return NULL;
	if (t) *t = &page_types[ref];
	if (p) *p = &pages[ref];

	return;
}

struct page *
ref2page_ptr(pageref_t ref)
{
	return &pages[ref];
}

/*
 * `page_resolve` finds the page corresponding to `offset`, validate
 * that it has the expected type, and that it is live. Returns either
 * `COS_ERR_WRONG_INPUT_TYPE` or `COS_ERR_WRONG_NOT_LIVE` in either
 * event, and `COS_RET_SUCCESS` on success. On success, the `page` and
 * `page_type` structures are returned in the corresponding
 * parameters.
 *
 * Assumes: the `offset` is a valid offset. This means that either it
 * is derived from a kernel-internal reference. As user-level only has
 * access capability namespaces that are component-local, user-level
 * should never use or know about these references.
 *
 * - `@offset` - which of the retypable pages; must be in bounds
 * - `@type` - expected type of the page
 * - `@kerntype` - expected kernel type of the page
 * - `@version` - `NULL`, or the expected version of the page
 * - `@page` - the returned page on success
 * - `@ptype` - the returned page type on success
 * - `@return` - the error or `COS_RET_SUCCESS`
 */
cos_retval_t
page_resolve(pageref_t offset, page_type_t type, page_kerntype_t kerntype, epoch_t *version, struct page **page, struct page_type **ptype)
{
	struct page_type *pt;
	struct page      *p;

	ref2page(offset, &p, &pt);

	//if (unlikely(epoch != pt->epoch)) return -COS_ERR_NOT_LIVE;
	if (unlikely(pt->type != type || pt->kerntype != kerntype)) return -COS_ERR_WRONG_INPUT_TYPE;

	/*
	 * Lets test if the target resource is live. This involves 1.
	 * checking if it has sufficient references, and 2. that the
	 * versioned pointer (should it be passed in) that is used to
	 * get to the page has not been made outdated. The former is
	 * relevant for resources for which each reference is tracked
	 * with a reference count, while the latter have a single
	 * reference while live, and can be made inaccessible
	 * (deallocated) by simply changing the version. The former
	 * are most resource-table resources, while the latter are
	 * components and component references.
	 *
	 * Note that we do *not* explicitly check that the reference
	 * count is non-zero here. Either, the capability used to
	 * derive `offset` exists, which means that the reference
	 * count is non-zero for resources that don't use the `epoch`,
	 * OR a resource uses the `epoch`, which is the ground truth
	 * for liveness (i.e. refcnt == 0 only if the version is
	 * incremented past all current reference's epochs).
	 */
	if (version != NULL && unlikely(*version != pt->epoch)) return -COS_ERR_NOT_LIVE;

	if (page)  *page  = p;
	if (ptype) *ptype = pt;

	return COS_RET_SUCCESS;
}

/***
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
 */

/**
 * `page_retype_from_untyped_reserve` attempts to begin the retyping
 * process of a resource from untyped into a specific other type
 * (kernel resource, or virtual memory). Will fail if the resource
 * hasn't quiesced (it is still "active", and *potentially* referenced
 * by logic on another core, or if it is not untyped.
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
 */
void
page_retype_from_untyped_commit(struct page_type *t, struct page *p, page_type_t type)
{
	/*
	 * Commit the updates. Now that the page's memory is
	 * initialized, make it accessible as the desired type.
	 */
	t->type = type;
	mem_barrier();
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
	if (t->refcnt != 1) return -COS_ERR_STILL_REFERENCED;
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

	COS_CHECK(CAPTBL_LOOKUP_TYPE(ct, pgtbl_cap, COS_CAP_TYPE_PGTBL_LEAF, COS_OP_DEALLOCATE, pgtbl));
	/* TODO: pay attention to the permissions */
	COS_CHECK(pgtbl_leaf_lookup(pgtbl->intern.ref, pgtbl_off, t, kt, COS_PGTBL_PERM_KERNEL, pgref));
	/* Note that this retype can only proceed after it checks the reference count */
	COS_CHECK(page_retype_to_untyped(*pgref));

	return COS_RET_SUCCESS;
}

/**
 * `resource_create_comp`: An example of resource creation for a
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
resource_create_comp(captbl_ref_t captbl_ref, pgtbl_ref_t pgtbl_ref, prot_domain_tag_t pd, vaddr_t entry_ip, pageref_t untyped_src_ref)
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
 * `resource_create_thd`: An example of resource creation for a
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
resource_create_thd(pageref_t sched_thd_ref, pageref_t tcap_thd_ref, pageref_t comp_ref, epoch_t epoch, thdid_t id, vaddr_t entry_ip, id_token_t sched_token, pageref_t untyped_src_ref)
{
	struct page_type *ptype, *sched_ptype, *tcap_ptype, *comp_ptype;
	struct page *thd_page, *tcap_page;
	struct component *comp;
	struct thread *thd;
	int tcap_self = untyped_src_ref == tcap_thd_ref;

	COS_CHECK(page_resolve(sched_thd_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD, NULL, NULL, &sched_ptype));
	if (!tcap_self) COS_CHECK(page_resolve(tcap_thd_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD, NULL, NULL, &tcap_ptype));
	COS_CHECK(page_resolve(comp_ref, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_COMP, &epoch, (struct page **)&comp, &comp_ptype));

	ref2page(untyped_src_ref, &thd_page, &ptype);
        /*
	 * Retype the page, and if we don't unreserve the capability.
         * Failure not possible after this point.
	 */
	COS_CHECK(page_retype_from_untyped_reserve(ptype, thd_page, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_THD));

        /*
	 * Take the references for the scheduler and tcap threads.
         * don't take a reference on the component, instead using
         * `epoch` to determine liveness.
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
					.component = (struct component_ref) {
						.pgtbl = comp->pgtbl,
						.captbl = comp->captbl,
						.pd_tag = comp->pd_tag,
						.epoch = epoch,
						.compref = comp_ref,
					},
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
 * `resource_create_restbl`: Create and initialize the page for a
 * resource table node. This includes all levels of capability-tables,
 * and page-tables.
 *
 * - `@kt` - the specific kernel type to create (must be a captbl/pgtbl type)
 * - `@untyped_src_ref` - the untyped memory to use for the component
 * - `@return` - normal return value denoting error (negative values), or success (zero)
 */
cos_retval_t
resource_create_restbl(page_kerntype_t kt, pageref_t untyped_src_ref)
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
