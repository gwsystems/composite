#ifndef PS_SMR_H
#define PS_SMR_H

#include <ps_slab.h>
#include <errno.h>
#include <ertrie.h>

struct ps_ns {
	ps_desc_t frontier;
	void *ert;
	struct ps_mem mem;
};

struct ps_slab *ps_slab_nsalloc(size_t sz, coreid_t coreid);
void ps_slab_nsfree(struct ps_slab *s, size_t sz, coreid_t coreid);

#define PS_NS_MEM_CREATE(name, objsz, nobjord)				\
PS_SLAB_CREATE_AFNS(name, PS_RNDPOW2(__ps_slab_objmemsz(objsz)), \
		    PS_RNDPOW2(__ps_slab_objmemsz(objsz)) * (1<<nobjord), \
		    0, ps_slab_nsalloc, ps_slab_nsfree)

/* #define PS_NS_CREATE(name, leafsz, leaforder, depth, internorder) ERT_CREATE_DEF(name, 3, 1024, , ) */

/* #ifdef NIL */
/* #define PS_NS_CREATE(name, sz, allocsz)					\ */
/* ERT_CREATE(name, ps_ns, 3, PS_PAGE_SIZE/PS_WORD, PS_WORD,	        \ */
/* 	   allocsz/PS_RNDPOW2(__ps_slab_objmemsz(sz))),			\ */
/* 	   PS_RNDPOW2(__ps_slab_objmemsz(sz)), NULL,			\ */
/* 	   ert_definit, ert_defget, ert_defisnull, ert_defset, allocfn, ert_defsetleaf, ert_defgetleaf, ert_defresolve) \ */
/* PS_SLAB_CREATE(name, PS_RNDPOW2(__ps_slab_objmemsz(sz)) - sizeof(struct ps_mheader), allocsz, 0) */
/* #endif */

/* void */
/* __ps_ns_free(struct ps_ns *ns, ps_desc_t id) */
/* { */
	
/* } */

#endif	/* PS_SMR_H */
