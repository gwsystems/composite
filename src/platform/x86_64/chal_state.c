#include <chal_regs.h>
#include <consts.h>
#include <chal_state.h>
#include <assert.h>

struct state_percore core_state[COS_NUM_CPU];
struct tlb_quiescence tlb_quiescence[COS_NUM_CPU] COS_CACHE_ALIGNED;

void
chal_state_init(void)
{
	int i;

	for (i = 0; i < COS_NUM_CPU; i++) {
		core_state[i].redzone = 0xDEADBEEF;
		core_state[i].gs_stack_ptr = (void *)&core_state[i].gs_stack_ptr;
		assert((u64_t)core_state[i].gs_stack_ptr - (u64_t)&core_state[i] == STATE_STACK_OFFSET);
		/* We should *not* use the registers set up on boot, so just zero them all out */
		core_state[i].registers = (struct regs) { 0 };
		/* We assume that the .globals are initialized separately */

		tlb_quiescence[i] = (struct tlb_quiescence) {
			.last_periodic_flush = 0,
			.last_mandatory_flush = 0,
		};
	}
}
