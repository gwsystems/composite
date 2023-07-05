#include <types.h>
#include <tss.h>
#include <chal_state.h>

struct tss tss[COS_NUM_CPU];

void
tss_init(const coreid_t cpu_id)
{
	tss[cpu_id].bitmap = 0xdfff;
	/*
	 * The stack really starts at `registers`, but that means that
	 * `sp` must point to the next highest address, which is
	 * `gs_stack_ptr`.
	 */
	tss[cpu_id].rsp0 = (u64_t)&chal_percore_state()->gs_stack_ptr;
}
