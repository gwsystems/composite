#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <vas_mgr.h>
#include <mem_mgr.h>

void cos_init(void)
{
	vaddr_t a, t;
	a = vas_mgr_expand(cos_spd_id(), cos_spd_id(), SERVICE_SIZE*2);
	printc("vas expand @ %x.\n", (unsigned int)a);
	if (a == 0) return;
	t = mman_get_page(cos_spd_id(), a+SERVICE_SIZE, MAPPING_RW);
	printc("mapped page %x, target %x\n", 
	       (unsigned int)t, (unsigned int)(a+SERVICE_SIZE));
	if (t != a+SERVICE_SIZE) printc("vas ERROR\n");
	
	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
