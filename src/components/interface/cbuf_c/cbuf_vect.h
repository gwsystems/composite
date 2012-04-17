#ifndef CBUF_VECT_H
#define CBUF_VECT_H

/* 
 * Since cbuf uses cvect data structure, reuse the code from cvect.h
 * as much as possible the only difference is in expand when request the
 * page from manager
 */

#include <cbuf_c.h>
#include <cos_debug.h>
#include <cos_component.h>

#include <cos_alloc.h>
#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

vaddr_t cbuf_c_register(spdid_t spdid, long cbid);

#define CBUF_VECT_CREATE_STATIC(name)	CVECT_CREATE_STATIC(name)

#define cbid_to_meta_idx(cid) ((cid-1) << 1)
#define meta_to_cbid_idx(mid) ((mid-1) >> 1)

static inline void *
cbuf_vect_lookup(cvect_t *v, long id)
{
	return cvect_lookup(v, id);
}

static inline void *
cbuf_vect_lookup_addr(cvect_t *v, long id)
{
	return cvect_lookup_addr(v, id);
}

static inline int
__cbuf_vect_expand_rec(struct cvect_intern *vi, const long id, const int depth)
{
	struct cvect_intern *new;

	if (depth > 1) {
		long n = id >> (CVECT_SHIFT * (depth-1));
		if (vi[n & CVECT_MASK].c.next == NULL) {
			new = (struct cvect_intern *)cbuf_c_register(cos_spd_id(), id);
			if (!new) return -1;
			vi[n & CVECT_MASK].c.next = new;
		}
		return __cbuf_vect_expand_rec(vi[n & CVECT_MASK].c.next, id, depth-1);
	}
	return 0;
}

static inline int 
__cbuf_vect_expand(cvect_t *v, long id)
{
	return __cbuf_vect_expand_rec(v->vect, id, CVECT_DEPTH);
}

static inline int 
cbuf_vect_expand(cvect_t *v, long id)
{
	return __cbuf_vect_expand(v, id);
}

#endif /* CBUF_VECT_H */
