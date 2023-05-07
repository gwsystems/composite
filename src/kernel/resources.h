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
} __attribute__((aligned(COS_PAGE_SIZE)));
