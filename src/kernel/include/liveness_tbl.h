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
#include "shared/util.h"
#include "ertrie.h"

#define LTBL_ENT_ORDER 10
#define LTBL_ENTS (1<<10)

 /* We use high 20 bits in the epoch to store the frame id in the page
  * table. Left 44 bits for epoch. */
#define LTBL_POLY_LENGTH  (20)
#define LTBL_EPOCH_LENGTH (64-LTBL_POLY_LENGTH)
#define LTBL_EPOCH_MASK   (((u64_t)1<<LTBL_EPOCH_LENGTH) - 1)
#define LTBL_POLY_MASK    (~LTBL_EPOCH_MASK)

struct liveness_entry {
	u64_t epoch;
	/* Here we store the timestamp of the deactivation call, so
	 * that we can support flexible quiescence period. */
	u64_t deact_timestamp;
} __attribute__((packed));

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
{ (void)d; (void)sz; (void)last_lvl; assert(0); return 0; }
static int __ltbl_isnull(struct ert_intern *a, void *accum, int leaf)
{ (void)accum; (void)leaf; (void)a; return 0; }
static int __ltbl_setleaf(struct ert_intern *a, void *data)
{ 
	u64_t old;
	(void)data; 
	old = ((struct liveness_entry *)a)->epoch; 
	/* FIXME: we need to support 64 bits */
	if (!cos_cas((unsigned long *)&(((struct liveness_entry *)a)->epoch), old, old + 1)) return -1;

	return 0;
}
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

/* Increment epoch, and update timestamp. */
static inline int
ltbl_expire(struct liveness_data *ld)
{
	struct liveness_entry *ent;
	u64_t old_v;

	ent = __ltbl_lkupan(LTBL_REF(), ld->id, __ltbl_maxdepth(), NULL);
	old_v = ent->epoch;
	ent->epoch = (old_v & LTBL_POLY_MASK) | (((old_v & LTBL_POLY_MASK) + 1) & LTBL_EPOCH_MASK);

	cos_inst_bar();

	rdtscll(ent->deact_timestamp);

	return 0;
}

/* 
 * After an object has been expired, when can it be freed (i.e. its
 * memory reclaimed)?
 */
static inline int
ltbl_isfreeable(struct liveness_data *ld, u64_t quiescence_period)
{
	struct liveness_entry *ent;
	u64_t ts;

	ent = __ltbl_lkupan(LTBL_REF(), ld->id, __ltbl_maxdepth(), NULL);
	rdtscll(ts);

	if (ent->deact_timestamp + quiescence_period < ts) return 1;

	return 0;
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

static inline u64_t
ltbl_get_timestamp(livenessid_t id)
{
	struct liveness_entry *ent;

	if (unlikely(id >= LTBL_ENTS)) return -EINVAL;
	ent = __ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth()+1, NULL);
	assert(ent);

	return ent->deact_timestamp;
}

static inline int
ltbl_poly_update(livenessid_t id, u32_t poly)
{
	struct liveness_entry *ent;

	if (unlikely(id >= LTBL_ENTS)) return -EINVAL;
	ent = __ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth(), NULL);

	/* If we have info stored in this liveness entry, do not allow
	 * the update. */
	if (unlikely(ent->epoch & LTBL_POLY_MASK)) return -EPERM;

	/* FIXME: atomic op? */
//	ent->poly = poly;
	rdtscll(ent->deact_timestamp);
	
	return 0;
}

static inline int
ltbl_poly_clear(livenessid_t id)
{
	struct liveness_entry *ent;

	if (unlikely(id >= LTBL_ENTS)) return -EINVAL;
	ent = __ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth(), NULL);

	/* FIXME: atomic op? */
//	ent->poly = 0;
//	rdtscll(ent->deact_timestamp);
	
	return 0;
}

static inline int
ltbl_timestamp_update(livenessid_t id)
{
	struct liveness_entry *ent;

	if (unlikely(id >= LTBL_ENTS)) return -EINVAL;

	ent = __ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth(), NULL);

	/* Barrier here to ensure tsc is taken after store
	 * instruction (avoid out-of-order execution). */
	cos_inst_bar();

	rdtscll(ent->deact_timestamp);

	return 0;
}

void ltbl_init(void);

#endif /* LIVENESS_TBL_H */
