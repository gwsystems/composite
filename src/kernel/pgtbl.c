#include <chal_consts.h>
#include <chal_atomics.h>
#include <cos_consts.h>
#include <cos_error.h>
#include <compiler.h>
#include <types.h>

#include <resources.h>
#include <pgtbl.h>

int
page_is_pgtbl(page_kerntype_t type)
{ return !(type < COS_PAGE_KERNTYPE_PGTBL_0 || type >= (COS_PAGE_KERNTYPE_PGTBL_0 + COS_PGTBL_MAX_DEPTH)); }

cos_retval_t
pgtbl_construct(pgtbl_ref_t top, uword_t offset, pgtbl_ref_t bottom, uword_t perm)
{
	struct pgtbl_internal  *top_node;
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

	COS_CHECK(page_resolve(bottom, COS_PAGE_TYPE_KERNEL, ktype + 1, NULL, NULL, &bottom_type));

	if (ktype == COS_PAGE_KERNTYPE_PGTBL_0) {
		struct pgtbl_top *top_node;

		COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, ktype, NULL, (struct page **)&top_node, &top_type));

		if (top_node->next[offset]) return -COS_ERR_ALREADY_EXISTS;

		/* Updates! */
		if (!cas64(&top_node->next[offset], 0, pgtbl_arch_entry_pack(bottom, perm))) return -COS_ERR_ALREADY_EXISTS;
	} else {
		struct pgtbl_internal *top_node;

		COS_CHECK(page_resolve(top, COS_PAGE_TYPE_KERNEL, ktype, NULL, (struct page **)&top_node, &top_type));

		if (top_node->next[offset]) return -COS_ERR_ALREADY_EXISTS;

		/* Updates! */
		if (!cas64(&top_node->next[offset], 0, pgtbl_arch_entry_pack(bottom, perm))) return -COS_ERR_ALREADY_EXISTS;
	}

	faa(&top_type->refcnt, 1);
	faa(&bottom_type->refcnt, 1);

	return COS_RET_SUCCESS;
}

/* TODO: remove bottom argument? */
cos_retval_t
pgtbl_deconstruct(pgtbl_ref_t top, uword_t offset)
{
	struct pgtbl_internal  *top_node;
	struct page_type       *top_type, *bottom_type;
	page_kerntype_t         ktype;
	uword_t                 bound;
	pgtbl_t                 entry;
	pgtbl_ref_t             bottom;
	uword_t                 perm;

	ref2page(top, (struct page **)&top_node, &top_type);
	ktype = top_type->kerntype;
	if (!page_is_pgtbl(ktype)) return -COS_ERR_WRONG_INPUT_TYPE;

	bound = ktype == COS_PAGE_KERNTYPE_PGTBL_0 ? COS_PGTBL_TOP_NENT : COS_PGTBL_INTERNAL_NENT;
	offset = COS_WRAP(offset, bound);

	entry = top_node->next[offset];
	if (pgtbl_arch_entry_empty(entry)) return -COS_ERR_RESOURCE_NOT_FOUND;
	pgtbl_arch_entry_unpack(entry, &bottom, &perm);
	/* Don't need to type-check this, as it is already linked into a kernel address */
	ref2page(bottom, NULL, &bottom_type);

	/* updates! */
	if (!cas64(&top_node->next[offset], entry, PGTBL_ARCH_ENTRY_NULL)) return -COS_ERR_NO_MATCH;
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
	if (pt_type->type != COS_PAGE_TYPE_KERNEL || pt_type->kerntype != COS_PAGE_KERNTYPE_PGTBL_LEAF) return -COS_ERR_WRONG_PAGE_TYPE;
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
	if (!cas64(&pt_node->next[offset], entry, PGTBL_ARCH_ENTRY_NULL)) return -COS_ERR_RESOURCE_NOT_FOUND;
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

/**
 * `pgtbl_copy` copies an entry from one pgtbl node, to another. This
 * aliases mappings, which creates shared memory in the case of the
 * entries pointing to virtual memory. This operates on *leaf* nodes.
 * For non-leaf nodes, `*_construct` should be used.
 *
 * - `@pgtbl_from` - page-table node to copy from
 * - `@pgtbl_off_from` - offset in that node to copy from
 * - `@pgtbl_to` - page-table node to copy to
 * - `@pgtbl_off_to` - offset in that node to copy into
 * - `@perm` - permissions which must be a subset of "from"'s permissions
 * - `@return` - typical return value, with various errors.
 */
cos_retval_t
pgtbl_copy(pgtbl_ref_t pgtbl_from, uword_t pgtbl_off_from, pgtbl_ref_t pgtbl_to, uword_t pgtbl_off_to, uword_t perm)
{
	struct pgtbl_internal *from, *to;
	pageref_t from_ref;
	uword_t from_perm;
	pgtbl_t ent_from, *ent_to_addr, ent_to;
	struct page_type *page;

	from = (struct pgtbl_internal *)ref2page_ptr(pgtbl_from);
	to = (struct pgtbl_internal *)ref2page_ptr(pgtbl_to);

	ent_from = from->next[COS_WRAP(pgtbl_off_from, COS_PGTBL_INTERNAL_NENT)];
	ent_to_addr = &to->next[COS_WRAP(pgtbl_off_to, COS_PGTBL_INTERNAL_NENT)];
	ent_to = *ent_to_addr;

	if (pgtbl_arch_entry_empty(ent_from)) return -COS_ERR_RESOURCE_NOT_FOUND;
	if (!pgtbl_arch_entry_empty(ent_to)) return -COS_ERR_ALREADY_EXISTS;

	pgtbl_arch_entry_unpack(ent_from, &from_ref, &from_perm);
	/* Potentially downgrade permissions. TODO: only allow manipulation of specific bits. */
	ref2page(from_ref, NULL, &page);

	ent_from = pgtbl_arch_entry_pack(from_ref, from_perm & perm);

	/* Update! */
	if (!cas64(ent_to_addr, ent_to, ent_from)) return -COS_ERR_CONTENTION;
	faa(&page->refcnt, 1);

	return COS_RET_SUCCESS;
}
