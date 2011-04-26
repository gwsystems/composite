#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <valloc.h>
#include <mem_mgr.h>

void cos_init(void)
{
	void *a, *b, *c;
	valloc_init(cos_spd_id());
	a = valloc_alloc(cos_spd_id(), cos_spd_id(), 16);
	assert(a);
	b = valloc_alloc(cos_spd_id(), cos_spd_id(), 64);
	assert(b);
	c = valloc_alloc(cos_spd_id(), cos_spd_id(), 32);
	assert(c);
	printc("%p, %p, %p\n", a, b, c);
	valloc_free(cos_spd_id(), cos_spd_id(), b, 64);
	valloc_free(cos_spd_id(), cos_spd_id(), a, 16);
	valloc_free(cos_spd_id(), cos_spd_id(), c, 32);
	return;
}
