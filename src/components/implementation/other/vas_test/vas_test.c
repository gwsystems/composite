#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <vas_mgr.h>
#include <mem_mgr.h>

void cos_init(void)
{
	vaddr_t a;
	a = vas_mgr_expand(cos_spd_id(), SERVICE_SIZE);
	printc("vas expand @ %x.\n", (unsigned int)a);
	if (a != mman_get_page(cos_spd_id(), a, 0)) {
		printc("ERROR: Could not map at new vas range\n");
	}

	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
