#include <cos_component.h>
#include <print.h>
#include <timed_blk.h>
#include <quarantine.h>

void cos_init(void)
{
	int ticks;
	spdid_t new_spd, target = 15; /* unit_cbuf1 */
	ticks = timed_event_block(cos_spd_id(), 2);

	/* TODO: which threads? */
	new_spd = quarantine_fork(cos_spd_id(), target);

	return;
}
