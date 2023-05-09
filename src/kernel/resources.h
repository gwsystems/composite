#pragma once

#include <cos_error.h>
#include <cos_consts.h>
#include <types.h>
#include <compiler.h>

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
cos_retval_t page_resolve(pageref_t offset, page_type_t type, page_kerntype_t kerntype, epoch_t *version,
                          struct page **page, struct page_type **ptype);
void         ref2page(pageref_t ref, struct page **p, struct page_type **t);
struct page *ref2page_ptr(pageref_t ref);
void         page_zero(struct page *p);

cos_retval_t resource_create_comp(captbl_ref_t captbl_ref, pgtbl_ref_t pgtbl_ref, prot_domain_tag_t pd, vaddr_t entry_ip, pageref_t untyped_src_ref);
cos_retval_t resource_create_thd(pageref_t sched_thd_ref, pageref_t tcap_thd_ref, pageref_t comp_ref, epoch_t epoch,
                                 thdid_t id, vaddr_t entry_ip, id_token_t sched_token, pageref_t untyped_src_ref);
cos_retval_t resource_create_restbl(page_kerntype_t kt, pageref_t untyped_src_ref);

cos_retval_t destroy_lookup_retype(captbl_t ct, cos_cap_t pgtbl_cap, uword_t pgtbl_off, page_type_t t,
                                   page_kerntype_t kt, pageref_t *pgref);
cos_retval_t resource_comp_destroy(pageref_t compref);
cos_retval_t resource_thd_destroy(pageref_t thdref);
