#include <cos_component.h>
#include <print.h>

#include <cinfo.h>
#include <sched.h>

void cos_init(void)
{
	int i;

	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spdid_t spdid = (spdid_t)i;
		void *hp = cinfo_get_heap_pointer(cos_spd_id(), spdid);
		printc("got heap -- id: %d, hp:%p\n", spdid, hp); 
	}
	printc("Done getting components heap pointers!\n");
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

