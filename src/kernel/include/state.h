#pragma once

#include <types.h>
#include <consts.h>
#include <cos_regs.h>

#include <compiler.h>
#include <component.h>
#include <resources.h>
#include <types.h>
#include <chal.h>

/*
 * The per-core, global data-structure. This includes the per-core
 * kernel stacks.
 */
struct state {
	/* Current thread information, consolidated onto a single cache-line here */
	struct thread *active_thread;
	uword_t invstk_head;
	captbl_t active_captbl;

	/* Scheduler information, assuming effectively a single scheduler */
	struct thread *sched_thread;
	cos_time_t timeout;

	/* Floating point state */
	int fpu_disabled;	      /* is access to floating point disabled (thus use will cause an exception) */
	struct thread *fpu_last_used; /* which thread does the current floating point state belong to? */
};

struct tlb_quiescence {
	/* Updated by timer. */
	u64_t last_periodic_flush;
	/* Updated by tlb flush IPI. */
	u64_t last_mandatory_flush;
	/* cacheline size padding. */
	u8_t __padding[COS_CACHELINE_SIZE - 2 * sizeof(u64_t)];
} __attribute__((aligned(COS_CACHELINE_SIZE), packed));

/*
 * We keep this data-structure separate from the other per-core state
 * as it can cause cache-coherency traffic due to multiple cores
 * accessing different entries, which we want to guarantee will *not*
 * happen for the core structures.
 */
extern struct tlb_quiescence tlb_quiescence[COS_NUM_CPU] COS_CACHE_ALIGNED;

COS_FORCE_INLINE static inline coreid_t
coreid(void)
{
	return 0;
}

COS_FASTPATH static inline cos_retval_t
component_activate(struct component_ref *comp)
{
	pageref_t ref;

	COS_CHECK(resource_compref_deref(comp, &ref));

	/* TODO: Load page-table, save captbl globally. */

	return COS_RET_SUCCESS;
}

#define PERCPU_GET(name) (&(state()-> name))

static inline liveness_t
liveness_now(void)
{
	/* TODO: actual liveness. */
	return 1;
}

/*
 * By default, liveness values for *live* resources are the highest
 * possible value, i.e. absolutely live as they cannot be quiesced.
 *
 * This simply helps handle the race between when we deactivate a
 * resource, and when we update its liveness value. In such a case, a
 * parallel access will not see the freed resource as quesced due to
 * this choice of the default.
 */
static inline liveness_t
liveness_live_default(void)
{
	return ~0;
}

static inline int
liveness_quiesced(liveness_t past)
{
	if (past == liveness_live_default()) return 0;
	/* TODO: actual liveness. */
	return 1;//liveness_now() > past;
}
