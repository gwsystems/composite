#include <cos_component.h>

__attribute__ ((weak))
void cos_upcall_fn(vaddr_t data_region, int thd_id, 
		   void *arg1, void *arg2, void *arg3)
{
	return;
}
