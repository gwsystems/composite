#include <chal_types.h>
#include <chal_atomics.h>
#include <compiler.h>
#include <cos_types.h>
#include <cos_consts.h>
#include <cos_error.h>
#include <types.h>
#include <state.h>

#include <capabilities.h>
#include <resources.h>
#include <captbl.h>

#define COS_CAPTBL_0_ENT_NULL 0

void
captbl_leaf_initialize(struct captbl_leaf *ct)
{
	int i;

	for (i = 0; i < COS_CAPTBL_LEAF_NENT; i++) {
		ct->capabilities[i] = (struct capability_generic) {
			.type = COS_CAP_TYPE_FREE,
			.liveness = 0,
			.operations = COS_OP_NIL,
			.intern = 0,
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

int
page_is_captbl(page_kerntype_t type)
{ return !(type < COS_PAGE_KERNTYPE_CAPTBL_0 || type >= (COS_PAGE_KERNTYPE_CAPTBL_0 + COS_CAPTBL_MAX_DEPTH)); }

/***
 * ### Capability Table Construction/Destruction
 *
 * `captbl_construct` and `captbl_deconstruct` create a reference, or
 * remove a reference from a node at level N in a capability table to
 * a node at level N + 1. These functions maintain the radix tries
 * that we use as the core of lookups. A few important properties
 * follow.
 *
 * All references between captbl nodes are a single word, so they are
 * easily updated with an atomic instruction. So no complicated
 * protocol is required to update them.
 *
 * A "NULL" entry uses `COS_CAPTBL_0_ENT_NULL`. This is an
 * optimization as it can be a pointer to a node with inaccessible
 * capability slots (i.e. all reserved). This enables us to avoid
 * conditionally checking references at each level (i.e. `if (ent ==
 * NULL) ...`).
 */

/**
 * `captbl_construct` adds a link in a capability-table node at level
 * N to a node at level N + 1, and updates the corresponding reference
 * counts.
 *
 * - `@top` - The top-level captbl node
 * - `@leaf` - The next-level captbl node
 * - `@offset` - The offset into `top` that should reference `leaf`
 * - `@return` - normal return with failures on mistypes, or missing mappings
 */
cos_retval_t
captbl_construct(captbl_ref_t top, captbl_ref_t leaf, uword_t offset)
{
	struct captbl_internal *top_node;
	struct captbl_leaf     *leaf_node;
	struct page_type       *top_type, *leaf_type;

	COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_0, NULL, (struct page **)&top_node, &top_type));
	COS_CHECK(page_resolve(leaf, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_1, NULL, (struct page **)&leaf_node, &leaf_type));
	offset = COS_WRAP(offset, COS_CAPTBL_INTERNAL_NENT);

	/* Updates! */
	if (!cas64(&top_node->next[offset], COS_CAPTBL_0_ENT_NULL, (u64_t)leaf_node)) return -COS_ERR_ALREADY_EXISTS;
	faa(&top_type->refcnt, 1);
	faa(&leaf_type->refcnt, 1);

	return COS_RET_SUCCESS;
}

/**
 * `captbl_deconstruct` removes a link in a capability-table node at
 * level N to a node at level N + 1, and updates the corresponding
 * reference counts.
 *
 * - `@top` - The top-level captbl node
 * - `@offset` - The offset into that node at which we're removing a mapping
 * - `@return` - normal return with failures on mistypes, or missing mappings
 */
cos_retval_t
captbl_deconstruct(captbl_ref_t top, uword_t offset)
{
	captbl_ref_t leaf;
	struct captbl_internal *top_node, *leaf_node;
	struct page_type       *top_type, *leaf_type;

	offset = COS_WRAP(offset, COS_CAPTBL_INTERNAL_NENT);

	COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, COS_PAGE_KERNTYPE_CAPTBL_0, NULL, (struct page **)&top_node, &top_type));
	leaf = top_node->next[offset];
	ref2page(leaf, (struct page **)&leaf_node, &leaf_type);
	/* assert: leaf must be proper type */

	/* Updates! */
	if (!cas64(&top_node->next[offset], leaf, COS_CAPTBL_0_ENT_NULL)) return -COS_ERR_CONTENTION;
	faa(&top_type->refcnt, -1);
	faa(&leaf_type->refcnt, -1);

	return COS_RET_SUCCESS;
}

struct capability_generic *
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

int
capability_is_captbl(struct capability_generic *c)
{
	return c->type <= COS_CAP_TYPE_CAPTBL_0 && c->type >= COS_CAP_TYPE_CAPTBL_LEAF;
}

int
capability_is_pgtbl(struct capability_generic *c)
{
	return c->type <= COS_CAP_TYPE_PGTBL_0 && c->type >= COS_CAP_TYPE_PGTBL_LEAF;
}

/***
 * ### Capability Slot Allocation/Deallocation
 *
 * Capability slots follow a state-machine similar to that for
 * resource retyping (see `resources.c` retyping information). The
 * *conceptual* states include:
 *
 * - A - active
 * - D - deactivated
 * - Q - quiescing
 * - R - reserved
 *
 * Active slots (A) can be accessed by any core's computations as the
 * appropriate type. Once deactivated (A -> D), a slot cannot be
 * accessed by any future capability table operations. The challenge
 * is that a parallel access that races with deactivation could
 * already see the active type. Thus we must await quiescence (D -> Q)
 * -- all parallel operations must complete before the slot is reused.
 * Once the slot has quiesced, we can use it to reference a new
 * resource and type. However, we can neither update the slot's memory
 * without updating the type as such updates might race with similar
 * updates on another core, nor can we update the type as other core's
 * computations might attempt to use the slot as that type before we
 * finish initializing the slot. Thus, we have the reserved state
 * which sets the type of the capability to `COS_CAP_TYPE_RESERVED`,
 * making it inaccessible to any parallel computations (Q -> R). Only
 * after the slot's memory is initialized do we update the slot's
 * type, making it active (R -> A). The functions that transition
 * through these states:
 *
 * - `captbl_cap_reserve`: D -> Q -> R
 * - `captbl_cap_activate`: R -> A
 * - `captbl_cap_deactivate`: A -> D
 */

/**
 * `captbl_cap_reserve` is used to reserve a capability slot in a
 * parallel-safe way so that no parallel operations can access the
 * capability slot. This enables us to initialize the slot. After it
 * is initialized, we treat its memory as read-only. At that point,
 * `captbl_cap_activate` finalizes the initialization, and makes the
 * slot visible to parallel and future accesses.
 *
 * Note that we cannot reserve a slot that since previously being
 * deactivated has not quiesced. We need to await all potential
 * parallel references to this slot completing before we modify the
 * slot's memory. So, awkward as it seems, we have to have a
 * liveness/quiescence check for slot deallocation in this allocation
 * function.
 *
 * - `@cap` - The capability slot
 * - `@return` - normal return value; errors on quiescence, or if the
 *   slot is already in use.
 */
static inline cos_retval_t
captbl_cap_reserve(struct capability_generic *cap)
{
	if (!liveness_quiesced(cap->liveness)) return -COS_ERR_NOT_QUIESCED;

	/* Update! */
	if (!cas32(&cap->type, COS_CAP_TYPE_FREE, COS_CAP_TYPE_RESERVED)) return -COS_ERR_ALREADY_EXISTS;

	return COS_RET_SUCCESS;
}

/**
 * `captbl_cap_activate` activates a previously reserved capability
 * slot. It initializes the capability with the type (`type`),
 * permitted operations (`operations`), and type-specific data
 * (`cap_data`). It is important that this is guaranteed to succeed so
 * that we don't need to roll back any additional operations (from
 * between when the slot was reserved, and when it is activated here).
 *
 * We force inlining here to remove the switch logic as `type` is
 * always passed in as a constant (i.e. simple constant propagation +
 * dead-code elimination).
 *
 * - `@cap` - capability slot to activate
 * - `@type` - desired type for the slot
 * - `@operations` - allowed/permitted  operations on the capability
 * - `@cap_data` - the type-specific initialized data for the slot
 *
 * This can't fail as we've already reserved the slot.
 */
COS_FORCE_INLINE static inline void
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

#define CAP_FALLTHROUGH(id0, id1, id2, id3)	\
	case id0: case id1: case id2: case id3:

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

	mem_barrier();
	/* Commit the changes, and make them accessible! */
	cap->type = type;

	return;
}

/**
 * `captbl_cap_deactivate` makes the capability slot inaccessible, and
 * frees it for future potential use after parallel quiescence.
 *
 * - `@captbl` - the captbl leaf node
 * - `@leaf_off` - the offset into that node of the slot to deactivate
 * - `@return` - normal return with errors on type mismatches, or races
 */
static cos_retval_t
captbl_cap_deactivate(struct captbl_leaf *captbl, uword_t leaf_off)
{
	struct capability_generic *cap;
	cos_cap_type_t t;

	cap = captbl_leaf_lookup(captbl, leaf_off);
	t   = cap->type;
	if (t == COS_CAP_TYPE_FREE || t == COS_CAP_TYPE_RESERVED) return -COS_ERR_RESOURCE_NOT_FOUND;

	/* parallel quiescence must be calculated from this point */
	cap->liveness = liveness_now();

	/*
	 * Can't destructively update the capability's fields aside
	 * from liveness and type due to parallel accesses.
	 */

	mem_barrier();
	/* Update! */
	if (!cas32(&cap->type, t, COS_CAP_TYPE_FREE)) return -COS_ERR_CONTENTION;

	return COS_RET_SUCCESS;
}

cos_retval_t
capability_remove(pageref_t captblref, uword_t off)
{
	struct captbl_leaf *ct;

	ref2page(captblref, (struct page **)&ct, NULL);

	return captbl_cap_deactivate(ct, off);
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

cos_retval_t
capability_copy(pageref_t captblref_to, uword_t off_to, pageref_t captblref_from, uword_t off_from, cos_op_bitmap_t ops)
{
	struct captbl_leaf *to, *from;

	ref2page(captblref_to, (struct page **)&to, NULL);
	ref2page(captblref_from, (struct page **)&from, NULL);

	return captbl_cap_copy(to, off_to, ops, from, off_from);
}

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
cap_comp_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t comp_src_ref)
{
	struct capability_component_intern i;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	struct component_ref ref;

	/*
	 * Build the reference to the component. Note that this makes
	 * no memory modifications, so we don't have undo any
	 * modifications if we error out later.
	 */
	COS_CHECK(resource_compref_create(comp_src_ref, &ref));

	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Update the capability to properly reference the resource */
	i = (struct capability_component_intern) {
		.component = ref,
	};

	/* Finish updating the capability to refer to the component structure. */
	captbl_cap_activate(cap, COS_CAP_TYPE_COMP, operations, &i);

	/*
	 * Note that the reference count of the component page hasn't
	 * changed as we're both removing (a page-table reference) and
	 * adding a (capability-table) reference.
	 */

	return COS_RET_SUCCESS;
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
cap_sinv_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, pageref_t comp_ref, vaddr_t entry_ip, inv_token_t token)
{
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	struct component_ref ref;
	struct capability_sync_inv_intern i;

	/* Build the reference to the server component */
	COS_CHECK(resource_compref_create(comp_ref, &ref));

	/* Get the slot we're adding the sinv into */
	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Update the capability to properly reference the resource */
	i = (struct capability_sync_inv_intern) {
		.token = token,
		.entry_ip = entry_ip,
		.component = ref,
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
cap_thd_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, cos_op_bitmap_t operations, pageref_t thd_ref)
{
	struct capability_resource_intern r;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	struct weak_ref weak_ref;

	COS_CHECK(resource_weakref_create(thd_ref, COS_PAGE_KERNTYPE_THD, &weak_ref));

	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Update the capability to properly reference the resource */
	r = (struct capability_resource_intern) {
		.ref  = weak_ref,
	};

	/* Finish updating the capability to refer to the component structure. */
	captbl_cap_activate(cap, COS_CAP_TYPE_THD, operations, &r);

	return COS_RET_SUCCESS;
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
cap_restbl_create(captbl_ref_t captbl_add_entry_ref, uword_t captbl_add_entry_off, page_kerntype_t kt, cos_op_bitmap_t operations, pageref_t restbl_ref)
{
	struct capability_resource_intern r;
	struct captbl_leaf *captbl_entry;
	struct capability_generic *cap;
	struct weak_ref weak_ref;

	COS_CHECK(resource_weakref_create(restbl_ref, kt, &weak_ref));

	captbl_entry = (struct captbl_leaf *)ref2page_ptr(captbl_add_entry_ref);
	cap = captbl_leaf_lookup(captbl_entry, COS_WRAP(captbl_add_entry_off, COS_CAPTBL_LEAF_NENT));
	/* Reserve the capability that we'll later populate */
	COS_CHECK(captbl_cap_reserve(cap));

	/* Update the capability to properly reference the resource */
	r = (struct capability_resource_intern) {
		.ref  = weak_ref,
	};

	/* Finish updating the capability to refer to the component structure. */
	captbl_cap_activate(cap, kt, operations, &r);

	return COS_RET_SUCCESS;
}

cos_retval_t
cap_hw_create(captbl_ref_t captbl_to_entry_ref, uword_t captbl_to_entry_off, captbl_ref_t captbl_from_entry_ref, uword_t captbl_from_entry_off, cos_op_bitmap_t ops)
{
	/* TODO */

	return COS_RET_SUCCESS;
}
