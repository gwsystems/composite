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

#include "shared/cos_types.h"
#include "ertrie.h"

#define LTBL_ENT_ORDER 10
#define LTBL_ENTS (1<<10)
struct liveness_entry {
	u64_t epoch, free_timestamp;
};
typedef struct liveness_entry ltbl_entry_t;
typedef u32_t livenessid_t;

struct liveness_data {
	u64_t epoch;
	livenessid_t id;
} __attribute__((packed));

/* 
 * TODO: Add the rdtsc for liveness and quiescence into the actual
 * liveness table entries themselves.
 */

/*
 * You should use the static pointer to the table instead of the one
 * returned from the ertrie API that calls this function so that the
 * compiler can statically determine it.
 */
static void *__ltbl_allocfn(void *d, int sz, int last_lvl)
{ (void)d; (void)sz; (void)last_lvl; assert(0); }
static int __ltbl_isnull(struct ert_intern *a, void *accum, int leaf)
{ (void)accum; (void)leaf; (void)a; return 0; }
/* FIXME: atomic operations */
static int __ltbl_setleaf(struct ert_intern *a, void *data)
{ (void)data; ((struct liveness_entry *)a)->epoch++; return 0; }
static void *__ltbl_getleaf(struct ert_intern *a, void *accum)
{ (void)accum; return &((struct liveness_entry *)a)->epoch; }

ERT_CREATE(__ltbl, ltbl, 1, 0, sizeof(int), LTBL_ENT_ORDER,		\
	   sizeof(struct liveness_entry), 0, ert_definit,		\
	   ert_defget, __ltbl_isnull, ert_defset, __ltbl_allocfn,	\
	   __ltbl_setleaf, __ltbl_getleaf, ert_defresolve);

extern struct liveness_entry __liveness_tbl[LTBL_ENTS];
#define LTBL_REF() ((struct ltbl *)__liveness_tbl)

static inline int
ltbl_isalive(struct liveness_data *ld)
{
	u64_t *epoch;

	epoch = __ltbl_lkupan(LTBL_REF(), ld->id, __ltbl_maxdepth()+1, NULL);
	if (unlikely(*epoch != ld->epoch)) return 0;
	return 1;
}

static inline int
ltbl_expire(struct liveness_data *ld)
{
	struct liveness_entry *ent;

	ent = __ltbl_lkupan(LTBL_REF(), ld->id, __ltbl_maxdepth(), NULL);
	ent->epoch++;
	/* FIXME: add the following */
	/* rdtscll(ts); */
	/* if (ent->free_timestamp > ts) return -EAGAIN; */
	/* ent->free_timestamp = ts + QUIESCE_PERIOD;  */
	/* return 0; */
	return 0;
}

/* 
 * After an object has been expired, when can it be freed (i.e. its
 * memory reclaimed)?
 */
static inline int
ltbl_isfreeable(struct liveness_data *ld)
{
	(void)ld;
	/* struct liveness_entry *ent; */
	/* ent = __ltbl_lkupan(__liveness_tbl, ld->id, __ltbl_maxdepth(), NULL); */
	/* rdtscll(ts); */
	/* if (ent->free_timestamp < ts) return 1; */
	/* return 0; */
	return 1;
}

static inline int
ltbl_get(livenessid_t id, struct liveness_data *ld)
{
	u64_t *e;
	if (unlikely(id >= LTBL_ENTS)) return -EINVAL;
	e = (u64_t*)__ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth()+1, NULL);
	assert(e);
	ld->epoch = *e;
	ld->id = id;
	return 0;
}

void ltbl_init(void);

#endif /* LIVENESS_TBL_H */
