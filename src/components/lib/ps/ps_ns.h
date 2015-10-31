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

#define PS_NS_CREATE(name, sz, allocsz) ERT_CREATE_DEF(name, 3, 1024, , )

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
