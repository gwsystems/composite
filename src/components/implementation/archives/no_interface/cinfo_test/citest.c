#include <cos_component.h>
#include <print.h>

#include <cinfo.h>
#include <sched.h>

void cos_init(void)
{
	void *hp = cos_get_heap_ptr();
	int i;

	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		struct cos_component_information *ci;
		spdid_t spdid;

		cos_set_heap_ptr((void*)(((unsigned long)hp)+PAGE_SIZE));
		spdid = cinfo_get_spdid(i);
		if (!spdid) break;

		if (cinfo_map(cos_spd_id(), (vaddr_t)hp, spdid)) {
			printc("Could not map cinfo page for %d.\n", spdid);
			return;
		}
		ci = hp;
		printc("mapped -- id: %ld, hp:%x, sp:%x\n", 
		       ci->cos_this_spd_id, (unsigned int)ci->cos_heap_ptr, 
		       (unsigned int)ci->cos_stacks.freelists[0].freelist);

		hp = cos_get_heap_ptr();
	}
	printc("Done mapping components information pages!\n");
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

