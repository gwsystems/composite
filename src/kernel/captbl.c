

#include <cos_consts.h>
#include <cos_error.h>
#include <cos_kern_types.h>

#define COS_CAPTBL_0_ENT_NULL 0

struct captbl_internal {
	captbl_t next[COS_CAPTBL_INTERNAL_NENT];
};

struct captbl_leaf {
	struct capability_generic capabilities[COS_PAGE_SIZE / sizeof(struct capability_generic)];
};

void
captbl_leaf_initialize(struct captbl_leaf *ct)
{
	int i;

	for (i = 0; i < COS_CAPTBL_LEAF_NENT; i++) {
		ct->capabilities[i] = (struct capability_generic) {
			.type = COS_CAP_TYPE_FREE,
			.liveness = 0,
			.intern = 0,
			.operations = COS_OP_MODIFY_UPDATE,
		};
	}
}

void
captbl_intern_initialize(struct captbl_internal *ct)
{
	int i;

	for (i = 0; i < COS_CAPTBL_INTERNAL_NENT; i++) {
		ct->next[i] = COS_CAPTBL_0_ENT_NULL;
	}
}


static inline int
page_is_captbl(page_kerntype_t type)
{ return !(type < COS_PAGE_KERNTYPE_CAPTBL_0 || type >= (COS_PAGE_KERNTYPE_CAPTBL_0 + COS_CAPTBL_MAX_DEPTH)); }

static cos_retval_t
captbl_construct(captbl_ref_t top, captbl_ref_t leaf, uword_t offset)
{
	struct captbl_internal *top_node;
	struct page_type       *top_type, *leaf_type;

	/* check arguments, and page-state/type */
	if (page_bounds_check(top) || page_bounds_check(leaf)) return -COS_ERR_OUT_OF_BOUNDS;
	COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_0, NULL, (struct page **)&top_node, &top_type));
	COS_CHECK(page_resolve(leaf, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_1, NULL, NULL, &leaf_type));

	offset = COS_WRAP(offset, COS_CAPTBL_INTERNAL_NENT);
	if (top_node->next[offset] != COS_CAPTBL_0_ENT_NULL) return -COS_ERR_ALREADY_EXISTS;

	/* Updates! */
	if (!cas64(&top_node->next[offset], COS_CAPTBL_0_ENT_NULL, leaf)) return -COS_ERR_ALREADY_EXISTS;
	faa(&top_type->refcnt, 1);
	faa(&leaf_type->refcnt, 1);

	return COS_RET_SUCCESS;
}

/* TODO: should we even need to pass in the second level node? */
static cos_retval_t
captbl_deconstruct(captbl_ref_t top, captbl_ref_t leaf, uword_t offset)
{
	struct captbl_internal *top_node, *leaf_node;
	struct page_type       *top_type, *leaf_type;

	/* check arguments, and page-state/type */
	if (page_bounds_check(top) || page_bounds_check(leaf)) return -COS_ERR_OUT_OF_BOUNDS;
	COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_0, NULL, (struct page **)&top_node, &top_type));
	COS_CHECK(page_resolve(leaf, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_1, NULL, (struct page **)&leaf_node, &leaf_type));
	offset = COS_WRAP(offset, COS_CAPTBL_INTERNAL_NENT);
	/* If the leaf isn't actually at this offset! */
	if (unlikely(top_node->next[offset] != leaf)) return -COS_ERR_NO_MATCH;

	/* Updates! */
	if (!cas64(&top_node->next[offset], leaf, COS_CAPTBL_0_ENT_NULL)) return -COS_ERR_NO_MATCH;
	faa(&top_type->refcnt, -1);
	faa(&leaf_type->refcnt, -1);

	return COS_RET_SUCCESS;
}

/**
 * `captbl_lookup` finds the capability in a captbl (`ct`) at
 * capability id `cap`. This function *cannot fail*, and the `cap` is
 * treated as wrapping in the capability namespace.
 */
COS_FORCE_INLINE static inline struct capability_generic *
captbl_lookup(captbl_t ct, cos_cap_t cap)
{
	/*
	 * NOTE: we're avoiding a bounds check on the `cap` here, and
	 * are instead wrapping around the captbl if `cap` is larger
	 * than the namespace. This is on the fastpath, so we want to
	 * avoid conditionals.
	 */
	int top_off  = (cap / (COS_CAPTBL_LEAF_NENT)) & (COS_CAPTBL_INTERNAL_NENT - 1);
	int leaf_off = cap & (COS_CAPTBL_LEAF_NENT - 1);

	struct captbl_internal *top  = (struct captbl_internal *)ct;
	struct captbl_leaf     *leaf = (struct captbl_leaf *)top->next[top_off];

	return &leaf->capabilities[leaf_off];
}

/**
 * `captbl_cap_typecheck` takes a capability, checks that it is live,
 * has the expected `type`, and has permissions that allow the
 * `required` operations. Note that `required` can be `0`, in which
 * the permissions are *not* checked. This is the fastpath for `sinv`
 * to avoid any checks (permissions for `sinv` are vacuous -- the
 * capability alone denotes a binary permission).
 */
static inline cos_retval_t
captbl_cap_typecheck(struct capability_generic* c, cos_cap_type_t type, cos_op_bitmap_t required)
{
	if (unlikely(c->type != type)) return -COS_ERR_WRONG_CAP_TYPE;
        /*
         * We first test required being zero to make it clear to the
         * compiler to omit this test in that case (i.e. the sinv
         * fastpath).
	 */
	if (unlikely(required && (c->operations & required) == required)) return -COS_ERR_INSUFFICIENT_PERMISSIONS;

	return COS_RET_SUCCESS;
}

/* Simple helper to do the cast to the super-type for us */
#define CAPTBL_LOOKUP_TYPE(ct, cap, type, required, cap_ret)                   \
  captbl_lookup_type(ct, cap, type, required,                                  \
                     (struct capability_generic **)&cap_ret)

/**
 * `captbl_lookup_type` does a capability lookup (looking up `cap` in
 * `ct`), and checks liveness, `type`, and if the capability provides
 * permissions to allow the `required` operations. Returns a normal
 * error/success, and the capability in `cap_ret`. Note that the
 * pointer might be populated even in the case of an error, in which
 * case you should ignore the value.
 */
static inline cos_retval_t
captbl_lookup_type(captbl_t ct, cos_cap_t cap, cos_cap_type_t type, cos_op_bitmap_t required, struct capability_generic **cap_ret)
{
	*cap_ret = captbl_lookup(ct, cap);

	return captbl_cap_typecheck(*cap_ret, type, required);
}

static inline struct capability_generic *
captbl_leaf_lookup(struct captbl_leaf *captbl, uword_t leaf_off)
{
	return &captbl->capabilities[COS_WRAP(leaf_off, COS_CAPTBL_LEAF_NENT)];
}

/**
 * `captbl_leaf_lookup_type` is like `captbl_lookup_type`, but does a
 * lookup in the last-level, leaf of the captbl.
 */
static inline cos_retval_t
captbl_leaf_lookup_type(struct captbl_leaf *captbl, uword_t leaf_off, cos_cap_type_t type, cos_op_bitmap_t required, struct capability_generic **cap_ret)
{
	*cap_ret = captbl_leaf_lookup(captbl, leaf_off);

	return captbl_cap_typecheck(*cap_ret, type, required);
}

/***
 * `captbl_cap_reserve` makes sure that we can activate the
 * capability, and reserve it for our initialization.
 */
static inline cos_retval_t
captbl_cap_reserve(struct capability_generic *cap)
{
	if (!liveness_quiesced(cap->liveness)) return -COS_ERR_NOT_QUIESCED;

	/* Update! */
	if (!cas16(&cap->type, COS_CAP_TYPE_FREE, COS_CAP_TYPE_RESERVED)) return -COS_ERR_ALREADY_EXISTS;

	return COS_RET_SUCCESS;
}

static inline int
capability_is_captbl(struct capability_generic *c)
{
	return c->type <= COS_CAP_TYPE_CAPTBL_0 && c->type >= COS_CAP_TYPE_CAPTBL_LEAF;
}

static inline int
capability_is_pgtbl(struct capability_generic *c)
{
	return c->type <= COS_CAP_TYPE_PGTBL_0 && c->type >= COS_CAP_TYPE_PGTBL_LEAF;
}

/**
 * `captbl_cap_unreserve` undos a previous reservation for a
 * capability slot. The core idea is that if we have to successfully
 * access multiple resources to create a capability, we might detect a
 * failure (i.e. one of those resources has an incorrect type), thus
 * need to undo the reservation as part of cleaning up the operation.
 */
static inline void
captbl_cap_unreserve(struct capability_generic *cap)
{
	/*
	 * No special logic here. The slot is reserved, so we can
	 * modify it directly.
	 */
	cap->type = COS_CAP_TYPE_FREE;
}

#define CAP_FALLTHROUGH(id0, id1, id2, id3)	\
	case id0: case id1: case id2: case id3:

/**
 * `captbl_cap_activate` activates a previously reserved capability
 * slot. It initializes the capability with the type (`type`),
 * permitted operations (`operations`), and type-specific data
 * (`cap_data`). It is important that this is guaranteed to succeed so
 * that we don't need to roll back any additional operations (from
 * between when the slot was reserved, and when it is activated here).
 */
static inline void
captbl_cap_activate(struct capability_generic *cap, cos_cap_type_t type, cos_op_bitmap_t operations, void *cap_data)
{
	cap->operations = operations;

	/*
	 * Cast the cap_data to the specific internal type required by
	 * the capability, and copy it into the capability.
	 */
#define CAP_INITIALIZE(typeid, typename)				\
	case typeid: {							\
		typename *specialized = (typename *)cap;		\
		typename##_intern *intern = cap_data;			\
									\
		specialized->intern = *intern;				\
		break;							\
	}

	switch (type) {
	CAP_INITIALIZE(COS_CAP_TYPE_SINV, struct capability_sync_inv)
	CAP_INITIALIZE(COS_CAP_TYPE_COMP, struct capability_component)
	CAP_INITIALIZE(COS_CAP_TYPE_HW,   struct capability_hw)
	/* Most capabilities fall through as they are simply a page/resource ref */
	CAP_FALLTHROUGH(COS_CAP_TYPE_PGTBL_0, COS_CAP_TYPE_PGTBL_1, COS_CAP_TYPE_PGTBL_2, COS_CAP_TYPE_PGTBL_3)
	CAP_FALLTHROUGH(COS_CAP_TYPE_CAPTBL_0, COS_CAP_TYPE_CAPTBL_1, COS_CAP_TYPE_VMCB, COS_CAP_TYPE_HWVM)
	CAP_FALLTHROUGH(COS_CAP_TYPE_SCB, COS_CAP_TYPE_DCB, COS_CAP_TYPE_ICB, COS_CAP_TYPE_RTCB)
	CAP_INITIALIZE(COS_CAP_TYPE_THD, struct capability_resource)
	}

	/* FIXME: barrier here */

	/* Commit the changes, and make them accessible! */
	cap->type = type;

	return;
}

static cos_retval_t
captbl_cap_copy(struct captbl_leaf *captbl_to, uword_t leaf_off_to, cos_op_bitmap_t operations, struct captbl_leaf *captbl_from, uword_t leaf_off_from)
{
	cos_cap_type_t type;
	struct capability_generic *cap_to, *cap_from;

	/* Validate that the capability we're copying supports copying */
	cap_from = captbl_leaf_lookup(captbl_from, leaf_off_from);
	type = cap_from->type;
	if (type == COS_CAP_TYPE_RESERVED || type == COS_CAP_TYPE_FREE) return -COS_ERR_RESOURCE_NOT_FOUND;
	/* Validate that we can copy, and that the operations don't upgrade permissions */
	if (!(cap_from->operations & COS_OP_CAP_COPY) || (operations & cap_from->operations) != operations) return -COS_ERR_INSUFFICIENT_PERMISSIONS;

	/* Prepare and initialize the capability we're copying into */
	COS_CHECK(captbl_leaf_lookup_type(captbl_to, leaf_off_to, COS_CAP_TYPE_FREE, 0, &cap_to));
	COS_CHECK(captbl_cap_reserve(cap_to));
	captbl_cap_activate(cap_to, type, operations, &cap_from->intern);

	return COS_RET_SUCCESS;
}



/***
 * Operations on capability entries themselves. These focus on
 * allocating capability entries, and deallocating them. They rely on
 * the functions specific to the different resources, but only call
 * them after the capability slot has been allocated successfully.
 *
 * Capability activation follows the general algorithm:
 *
 * - Lookup the capability entry
 * - Check that the entry is not still live, or error out.
 * - Attempt to reserve the entry using an atomic instruction. At this
 *   point, we are the only core that can access the slot.
 * - Reserve the referenced resources (backed by a page) by reserving
 *   it. Similarly uses an atomic instruction, ensuring we are the
 *   only core that can access the page.
 * - Initialize the capability slot to reference the resource and
 *   "freeze" the capability representation.
 * - Activate the capability by typing it to the appropriate type,
 *   thus making it accessible.
 *
 * Capability deactivation is simpler as it is not responsible for
 * resource deallocation. Thus it does the following:
 *
 * - Lookup the capability entry.
 * - If the type holds a refcnt, decrement it!
 * - Set the liveness of the entry to now.
 * - Update the capability type to "free".
 *
 * FIXME: should first set type to RETYPING, then to free to avoid races.
 */

/**
 * `cap_create_comp`: Create the capability structure for a new
 * component resource. Components are created in a new capability slot
 * (`captbl_add_entry_ref` at offset `captbl_add_entry_off`) from
 *
 * 1. a page-table (`pgtbl_ref`),
 * 2. a capability-table (`captbl_ref`), and
 * 3. an untyped page we're using for the component (`untyped_src_ref`).
 */
cos_retval_t
cap_create_comp(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, captbl_ref_t captbl_ref,
		pgtbl_ref_t pgtbl_ref, prot_domain_tag_t pd, vaddr_t entry_ip, pgtbl_ref_t untyped_src_ref)
{
	struct capability_component_intern i;
	cos_retval_t ret;
	struct page_type *ptype;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;

	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Set up the resource's memory... */
	COS_CHECK_THROW(resource_create_comp(captbl_ref, pgtbl_ref, pd, entry_ip, untyped_src_ref), ret, err);

	ref2page(untyped_src_ref, NULL, &ptype);
	/* Update the capability to properly reference the resource */
	i = (struct capability_component_intern) {
		.comp  = untyped_src_ref,
		.epoch = ptype->epoch,
	};

	/* Finish updating the capability to refer to the component structure. */
	captbl_cap_activate(cap, COS_CAP_TYPE_COMP, operations, &i);

	/*
	 * Note that the reference count of the component page hasn't
	 * changed as we're both removing (a page-table reference) and
	 * adding a (capability-table) reference.
	 */

	return COS_RET_SUCCESS;
err:
	captbl_cap_unreserve(cap);

	return ret;
}

/**
 * `cap_create_sinv` provides synchronous invocation creation, which
 * centers around capability creation. Simply create the capability
 * referencing the associated component. This does *not* increase the
 * reference count of the component. Rather, it uses a versioned
 * counter to understand if the reference is valid, or outdated.
 *
 * - `@captbl_add_entry_ref` - captbl node in which to add entry
 * - `@captbl_add_entry_off` - offset of entry into that captbl node
 * - `@comp_ref` - component to which sinv is made
 * - `@entry_ip` - entry instruction pointer into that component
 * - `@token` - the invocation token passed to the component on invocation
 * - `@return` - typical success or error
 */
cos_retval_t
cap_create_sinv(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, pageref_t comp_ref, vaddr_t entry_ip, inv_token_t token)
{
	cos_retval_t ret = COS_RET_SUCCESS;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	struct component *comp;
	struct page_type *ptype;
	struct capability_sync_inv_intern i;

	/* get the component we're creating the sinv into */
	ref2page(comp_ref, (struct page **)&comp, &ptype);

	/* Get the slot we're adding the sinv into */
	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Update the capability to properly reference the resource */
	i = (struct capability_sync_inv_intern) {
		.token = token,
		.entry_ip = entry_ip,
		.component = (struct component_ref) {
			.pgtbl = comp->pgtbl,
			.captbl = comp->captbl,
			.epoch = epoch_copy(ptype),
			.pd_tag = comp->pd_tag,
			.compref = comp_ref,
		}
	};

	/* Finish updating the capability to the sinv type. */
	captbl_cap_activate(cap, COS_CAP_TYPE_SINV, 0, &i);

	/*
	 * Note that the reference count of the component page hasn't
	 * changed as we're both removing (a page-table reference) and
	 * adding a (capability-table) reference.
	 */

	return COS_RET_SUCCESS;
}

/**
 * `cap_create_thread`: Create the capability structure for a new
 * thread resource. Threads are created in a new capability slot
 * (`captbl_add_entry_ref` at offset `captbl_add_entry_off`) from
 *
 * 1. a scheduler thread,
 * 2. a component that the thread is to start executing in,
 * 3. a tcap if we aren't using the scheduler thread's,
 * 4. an untyped page we're using for the thread (`untyped_src_ref`), and
 * 5. the assorted pieces of state that the thread wants.
 */
cos_retval_t
cap_create_thd(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t sched_ref, pageref_t tcap_ref, pageref_t comp_ref, epoch_t epoch, thdid_t id, id_token_t token, pgtbl_ref_t untyped_src_ref)
{
	struct capability_resource_intern r;
	cos_retval_t ret;
	struct page_type *ptype;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	prot_domain_tag_t pd;
	vaddr_t entry_ip;

	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Set up the resource's memory... */
	COS_CHECK_THROW(resource_create_thd(sched_ref, tcap_ref, comp_ref, epoch, id, entry_ip, token, untyped_src_ref), ret, err);

	ref2page(untyped_src_ref, NULL, &ptype);
	/* Update the capability to properly reference the resource */
	r = (struct capability_resource_intern) {
		.ref  = untyped_src_ref,
	};

	/* Finish updating the capability to refer to the component structure. */
	captbl_cap_activate(cap, COS_CAP_TYPE_THD, operations, &r);

	return COS_RET_SUCCESS;
err:
	captbl_cap_unreserve(cap);

	return ret;
}

/**
 * `cap_create_restbl`: Create the capability structure for a new
 * resource table resource. This function is generic across
 * page-tables and capability-table nodes (at various levels), thus
 * requires the type we're constructing as an argument.
 *
 * - `@captbl_add_entry_ref` - the capability table node to add this restbl into
 * - `@captbl_add_entry_off` - the offset at which to add it
 * - `@kt` - the resource type (only pgtbl and captbl types allowed)
 * - `@operations` - allowed operations on the capability
 * - `@untyped_src_ref` - the reference to the untyped memory to use for the restbl node.
 * - `@return` - `COS_RET_SUCCESS` or a negative error value.
 */
cos_retval_t
cap_create_restbl(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, page_kerntype_t kt, cos_op_bitmap_t operations, pgtbl_ref_t untyped_src_ref)
{
	struct capability_resource_intern r;
	cos_retval_t ret;
	struct page_type *ptype;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	prot_domain_tag_t pd;
	vaddr_t entry_ip;

	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Set up the resource's memory... */
	COS_CHECK_THROW(resource_create_restbl(kt, untyped_src_ref), ret, err);

	ref2page(untyped_src_ref, NULL, &ptype);
	/* Update the capability to properly reference the resource */
	r = (struct capability_resource_intern) {
		.ref  = untyped_src_ref,
	};

	/* Finish updating the capability to refer to the component structure. */
	captbl_cap_activate(cap, kt, operations, &r);

	return COS_RET_SUCCESS;
err:
	captbl_cap_unreserve(cap);

	return ret;
}

static cos_retval_t
cap_deactivate(struct captbl_leaf *captbl, uword_t leaf_off, cos_cap_type_t type)
{
	struct capability_generic *cap;
	cos_cap_type_t t;
	pageref_t resource;
	int has_refcnt = 1;
	struct page_type *pagetype;

	cap = captbl_leaf_lookup(captbl, leaf_off);
	t   = cap->type;
	if (t != type) return -COS_ERR_WRONG_CAP_TYPE;
	if (t == COS_CAP_TYPE_FREE || t == COS_CAP_TYPE_RESERVED) return -COS_ERR_RESOURCE_NOT_FOUND;

	switch (t) {
	case COS_CAP_TYPE_SINV:
	case COS_CAP_TYPE_HW:
		/* No refcnts */
		has_refcnt = 0;
		break;
	case COS_CAP_TYPE_COMP:
		resource = ((struct capability_component *)cap)->intern.comp;
		break;
	CAP_FALLTHROUGH(COS_CAP_TYPE_PGTBL_0, COS_CAP_TYPE_PGTBL_1, COS_CAP_TYPE_PGTBL_2, COS_CAP_TYPE_PGTBL_3)
	CAP_FALLTHROUGH(COS_CAP_TYPE_CAPTBL_0, COS_CAP_TYPE_CAPTBL_1, COS_CAP_TYPE_VMCB, COS_CAP_TYPE_HWVM)
	CAP_FALLTHROUGH(COS_CAP_TYPE_SCB, COS_CAP_TYPE_DCB, COS_CAP_TYPE_ICB, COS_CAP_TYPE_RTCB)
	case COS_CAP_TYPE_THD:
		resource = ((struct capability_resource *)cap)->intern.ref;
		break;
	}

	if (has_refcnt) {
		ref2page(resource, NULL, &pagetype);
		faa(&pagetype->refcnt, -1);
	}
	/* parallel quiescence must be calculated from this point */
	cap->liveness = liveness_now();

	/* Can't destructively update the capability aside from liveness and type due to parallel accesses. */

	/* Update! */
	if (!cas16(&cap->type, t, COS_CAP_TYPE_FREE)) return -COS_ERR_ALREADY_EXISTS;

	return COS_RET_SUCCESS;
}
