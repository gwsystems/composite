#include <cos_component.h>

/* 
 * This is initialized at load time with the spd id of the current
 * spd, and is passed into all system calls to identify the calling
 * service.
 */
long cos_this_spd_id = 0;

__attribute__ ((weak))
void cos_upcall_fn(vaddr_t data_region, int thd_id, 
		   void *arg1, void *arg2, void *arg3)
{
	return;
}
