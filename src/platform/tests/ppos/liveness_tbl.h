/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * This is a simple table of epoch counters.
 */

#ifndef LIVENESS_TBL_H
#define LIVENESS_TBL_H

#include <ertrie.h>

#define LTBL_ENTS    4096
typedef u64_t        ltbl_entry_t
#define LTBL_ENT_SZ  sizeof(ltbl_entry_t)
extern LTBL_ENT_TYPE __liveness_tbl[LTBL_ENTS];
struct liveness_data {
	ltbl_entry_t epoch;
	u32_t id;
};

/* 
 * You should use the static pointer to the table instead of the one
 * returned from the ertrie API that calls this function so that the
 * compiler can statically determine it.
 */
static void *__ltbl_allocfn(void *d, int sz, int last_lvl) { assert(0); }
static int __ltbl_isnull(struct ert_intern *a, void *accum, int leaf)
{ (void)accum; (void)leaf; (void)a; return 0; }
/* FIXME: atomic operations */
static void __ltbl_setleaf(struct ert_intern *a, void *data)
{ (void)data; (*(u32_t*)a)++; }

/* FIXME: should be 2^16 entries, not 4096 */
ERT_CREATE(__ltbl, ltbl, 1, 0, LTBL_ENTS, sizeof(ltbl_entry_t), 0,	\
	ert_definit, ert_defget, __ltbl_isnull, ert_defset,	\
	__ltbl_allocfn, __ltbl_setleaf, ert_defgetleaf, ert_defresolve); 

void ltbl_expire(struct liveness_data *ld)
{ __ltbl_expandn(__liveness_tbl, ld->id, __ltbl_maxdepth()+1, NULL, NULL, NULL); }

int 
ltbl_isalive(struct liveness_data *ld)
{ 
	ltbl_entry_t epoch;

	epoch = (ltbl_entry_t)__ltbl_lkupan(__liveness_tbl, ld->id, __ltbl_maxdepth()+1, NULL); 
	if (unlikely(epoch != ld->epoch)) return 0;
	return 1;
}

void
ltbl_get(u32_t id, struct liveness_data *ld)
{
	assert(id < LTBL_NENTS);
	ld->epoch = (ltbl_entry_t)__ltbl_lkupan(__liveness_tbl, id, __ltbl_maxdepth()+1, NULL);
	ld->id = id;
}


void
ltbl_init(void)
{
	int i;
	for (i = 0 ; i < ; i++) __liveness_tbl[i] = 0;
}

#endif /* LIVENESS_TBL_H */
