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

#define LTBL_ENT_ORDER 20
#define LTBL_ENTS (1 << LTBL_ENT_ORDER)

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A"(val))
#endif

/* We need 64-bit for each of the field in liveness entry. */
struct liveness_entry {
	u64_t epoch;
	/* Here we store the timestamp of the deactivation call, so
	 * that we can support flexible quiescence period. */
	u64_t deact_timestamp;
} __attribute__((packed));

typedef struct liveness_entry ltbl_entry_t;
typedef u32_t                 livenessid_t;

struct liveness_data {
	u64_t        epoch;
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
static void *
__ltbl_allocfn(void *d, int sz, int last_lvl)
{
	(void)d;
	(void)sz;
	(void)last_lvl;
	assert(0);
	return 0;
}
static int
__ltbl_isnull(struct ert_intern *a, void *accum, int leaf)
{
	(void)accum;
	(void)leaf;
	(void)a;
	return 0;
}
static int
__ltbl_setleaf(struct ert_intern *a, void *data)
{
	u64_t old, new, *ptr;
	(void)data;

	ptr = &(((struct liveness_entry *)a)->epoch);
	old = *ptr;
	new = old + 1;

	/*FIXME: 64-bit op*/
	if (!cos_cas((unsigned long *)ptr, (unsigned long)old, (unsigned long)new)) return -ECASFAIL;

	return 0;
}
static void *
__ltbl_getleaf(struct ert_intern *a, void *accum)
{
	(void)accum;
	return &((struct liveness_entry *)a)->epoch;
}

ERT_CREATE(__ltbl, ltbl, 1, 0, sizeof(int), LTBL_ENT_ORDER, sizeof(struct liveness_entry), 0, ert_definit, ert_defget,
           __ltbl_isnull, ert_defset, __ltbl_allocfn, __ltbl_setleaf, __ltbl_getleaf, ert_defresolve);

extern struct liveness_entry __liveness_tbl[LTBL_ENTS];
#define LTBL_REF() ((struct ltbl *)__liveness_tbl)

static inline int
ltbl_isalive(struct liveness_data *ld)
{
	u64_t *epoch;

	epoch = __ltbl_lkupan(LTBL_REF(), ld->id, __ltbl_maxdepth() + 1, NULL);
	if (unlikely(*epoch != ld->epoch)) return 0;
	return 1;
}

/* Increment epoch, and update timestamp. */
static inline int
ltbl_expire(struct liveness_data *ld)
{
	struct liveness_entry *ent;
	u64_t                  old_v;

	ent   = __ltbl_lkupan(LTBL_REF(), ld->id, __ltbl_maxdepth(), NULL);
	old_v = ent->epoch;

	rdtscll(ent->deact_timestamp);
	cos_mem_fence();
	//	cos_inst_bar();

	// FIXME: 64-bit op
	if (!cos_cas((unsigned long *)&ent->epoch, (unsigned long)old_v, (unsigned long)(old_v + 1))) return -ECASFAIL;

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
	u64_t                  ts;

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

	e = (u64_t *)__ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth() + 1, NULL);
	assert(e);

	ld->epoch = *e;
	ld->id    = id;

	return 0;
}

static inline int
ltbl_get_timestamp(livenessid_t id, u64_t *ts)
{
	struct liveness_entry *ent;

	if (unlikely(id >= LTBL_ENTS)) return -EINVAL;
	ent = __ltbl_lkupan(LTBL_REF(), id, __ltbl_maxdepth() + 1, NULL);
	assert(ent);

	*ts = ent->deact_timestamp;

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
	//	cos_inst_bar();
	cos_mem_fence();

	rdtscll(ent->deact_timestamp);

	return 0;
}

void ltbl_init(void);

#endif /* LIVENESS_TBL_H */
