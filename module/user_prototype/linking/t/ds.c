#include <cos_component.h>
#include <cos_debug.h>

extern void sched_exit(void);

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	assert(0);

	return;
}

int bin(void) {
	sched_exit();
	return 0;
}
