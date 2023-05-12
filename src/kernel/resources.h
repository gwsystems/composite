#pragma once

#include <cos_error.h>
#include <cos_consts.h>
#include <types.h>
#include <compiler.h>
#include <component.h>

struct page_type {
	page_type_t     type;  	   /* page type */
	page_kerntype_t kerntype;  /* page's kernel type, assuming type == kernel */
	coreid_t        coreid;	   /* resources that are bound to a core (e.g. thread) */
	epoch_t         epoch;	   /* increment to invalidate pointers to the page */
	liveness_t      liveness;  /* tracking if there are potential parallel references */
	refcnt_t        refcnt;	   /* reference count */
};

struct page {
	uword_t words[COS_PAGE_SIZE / sizeof(uword_t)];
} COS_PAGE_ALIGNED;

int          page_bounds_check(pageref_t ref);
void         page_zero(struct page *p);

cos_retval_t resource_comp_create(captbl_ref_t captbl_ref, pgtbl_ref_t pgtbl_ref, prot_domain_tag_t pd, vaddr_t entry_ip, pageref_t untyped_src_ref);
cos_retval_t resource_comp_destroy(pageref_t compref);
cos_retval_t resource_compref_create(pageref_t compref, struct component_ref *r);

cos_retval_t resource_thd_create(pageref_t sched_thd_ref, pageref_t tcap_thd_ref, pageref_t comp_ref,
                                 thdid_t id, vaddr_t entry_ip, id_token_t sched_token, pageref_t untyped_src_ref);
cos_retval_t resource_thd_destroy(pageref_t thdref);

cos_retval_t resource_restbl_create(page_kerntype_t kt, pageref_t untyped_src_ref);

cos_retval_t destroy_lookup_retype(captbl_t ct, cos_cap_t pgtbl_cap, uword_t pgtbl_off, page_type_t t,
                                   page_kerntype_t kt, pageref_t *pgref);

cos_retval_t resource_weakref_create(pageref_t resource_ref, page_kerntype_t expected_kerntype, struct weak_ref *wr);

/*
 * Faster paths for type checking, and resource dereferencing that
 * likely want to be inlined (to remove quite a bit of the code based
 * on arguments).
 */

extern struct page_type page_types[COS_NUM_RETYPEABLE_PAGES] COS_PAGE_ALIGNED;
extern struct page      pages[COS_NUM_RETYPEABLE_PAGES];

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
COS_FORCE_INLINE static inline void
ref2page(pageref_t ref, struct page **p, struct page_type **t)
{
	/*
	 * Page references should be implementation-internal. Use
	 * verification invariants to guarantee they are valid
	 * values (i.e. ref is within bounds).
	 */
	if (t) *t = &page_types[ref];
	if (p) *p = &pages[ref];

	return;
}

static inline struct page *
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
COS_FORCE_INLINE static inline cos_retval_t
page_resolve(pageref_t offset, page_type_t type, page_kerntype_t kerntype, epoch_t *version, struct page **page, struct page_type **ptype)
{
	struct page_type *pt;
	struct page      *p;

	ref2page(offset, &p, &pt);

	if (unlikely(pt->type != type || pt->kerntype != kerntype)) return -COS_ERR_WRONG_INPUT_TYPE;

	if (page)  *page  = p;
	if (ptype) *ptype = pt;

	return COS_RET_SUCCESS;
}

static inline cos_retval_t
resource_weakref_deref(struct weak_ref *wr, pageref_t *resource)
{
	struct page_type *pt;

	ref2page(wr->ref, NULL, &pt);
	if (unlikely(wr->epoch != pt->epoch)) return -COS_ERR_NOT_LIVE;

	*resource = wr->ref;

	return COS_RET_SUCCESS;
}

static inline pageref_t
resource_weakref_force_deref(struct weak_ref *wr)
{
	return wr->ref;
}
