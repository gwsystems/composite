#pragma once

#include "chal_consts.h"
#include "compiler.h"
#include <component.h>
#include <chal_regs.h>

COS_FORCE_INLINE static inline coreid_t
coreid(void)
{
	return 0;
}

COS_FASTPATH static inline cos_retval_t
component_activate(struct component_ref *comp)
{
	pageref_t ref;

	COS_CHECK(component_ref_deref(comp, &ref));

	/* TODO: Load page-table, save captbl globally. */

	return COS_RET_SUCCESS;
}

struct state_percore {
	struct regs registers;	/* must be the first item as we're going to use stack ops to populate these */

	/* Current thread information, consolidated onto a single cache-line here */
	struct thread *active_thread;
	uword_t invstk_head;
	captbl_t active_captbl;

	/* Scheduler information, assuming effectively a single scheduler */
	struct thread *sched_thread;
} COS_CACHE_ALIGNED;

COS_FORCE_INLINE static inline struct state_percore *
state(void)
{
	extern struct state_percore core_state[COS_NUM_CPU];

	return &core_state[0];
}

static inline liveness_t
liveness_now()
{
	/* TODO: actual liveness. */
	return 0;
}

static inline int
liveness_quiesced(liveness_t past)
{
	/* TODO: actual liveness. */
	return liveness_now() > past;
}
