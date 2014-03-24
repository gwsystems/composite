/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * This is a capability table implementation based on embedded radix
 * trees.
 */

#ifndef CAPTBL_H
#define CAPTBL_H

#include <ertrie.h>
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

#define CAPTBL_DEPTH    2
#define CAPTBL_INTERNSZ sizeof(int*)
#define CAPTBL_LEAFSZ   sizeof(struct cap_min)
typedef enum {
	CAP_FREE, 		/* not allocated */
	CAP_SCOMM,		/* synchronous communication */
	CAP_ASCOMM_SND,		/* async communication; sender */
	CAP_ASCOMM_RCV,         /* async communication; receiver */
	CAP_THD,                /* thread */
} cap_t;
struct cap_poly {
	cap_t type;
	u8_t  order;
};
struct cap_min {
	struct cap_poly h;
	char padding[(4*sizeof(int))-sizeof(struct cap_poly)];
};

static void *
__captbl_allocfn(void *d, int sz, int last_lvl)
{
	(void)last_lvl;
	void **mem = d; 	/* really a pointer to a pointer */
	void *m = *mem;
	assert(sz <= PAGE_SIZE/2);
	*mem = NULL;		/* NULL so we don't do mult allocs */

	return m;
}

ERT_CREATE(__captbl, captbl, CAPTBL_DEPTH,				\
	   9 /* PAGE_SIZE/(2*(CAPTBL_DEPTH-1)*CAPTBL_INTERNSZ) */, CAPTBL_INTERNSZ, \
	   7 /*PAGE_SIZE/(2*CAPTBL_LEAFSZ) */, CAPTBL_LEAFSZ, \
	   NULL, ert_definit, ert_defget, ert_defisnull, ert_defset,  \
	   __captbl_allocfn, ert_defsetleaf, ert_defgetleaf, ert_defresolve); 

static struct captbl *captbl_alloc(void *mem) { return __captbl_alloc(&mem); }

/* 
 * This function is the fast-path used for capability lookup in the
 * invocation path.
 */
static inline struct cap_poly *
captbl_lkup(struct captbl *t, unsigned long cap)
{ 
	if (unlikely(cap > __captbl_maxid())) return NULL;
	return __captbl_lkupan(t, cap, CAPTBL_DEPTH, NULL); 
}

static inline int
captbl_expand(struct captbl *t, unsigned long cap, void *memctxt)
{
	int ret;

	if (unlikely(cap > __captbl_maxid())) return -1;
	ret = __captbl_expand(t, cap, NULL, &memctxt, NULL);
	if (unlikely(ret)) return -1;
	if (memctxt)       return  1;
	return 0;
}

#endif /* CAPTBL_H */
