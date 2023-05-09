#include <cos_consts.h>
#include <cos_error.h>
#include <compiler.h>
#include <types.h>
#include <atomics.h>

#include <resources.h>
#include <pgtbl.h>

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

int
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
cos_retval_t
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
