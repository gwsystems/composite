#include "cos_component.h"
#include "cos_synchronization.h"

void cos_lock_take(cos_lock_t *s)
{
	unsigned short int thd, gen;
	unsigned short int curr_thd = (unsigned short int)cos_get_thd_id();

	do {
		__asm__ __volatile__("call cos_atomic_user1"
				     : "=c" (gen), "=D" (thd) 
				     : "a" (s), "b" (curr_thd)
				     : "memory", "d");
		if (NULL != thd) {
			cos_lock_component_take_extern(thd, gen, s->lock_id);
		}
	} while (NULL != thd);

	return;
}

void cos_lock_release(cos_lock_t *s)
{
	unsigned short int gen, thd;
	unsigned short int curr_thd = (unsigned short int)cos_get_thd_id();

	__asm__ __volatile__("call cos_atomic_user2"
			     : "=c" (gen), "=D" (thd)
			     : "a" (s), "b" (curr_thd)
			     : "memory");
	/* release those threads waiting on the lock by updating current gen */
	if (thd != curr_thd) {
		cos_lock_component_release_extern(thd, gen, s->lock_id);
	}

	return;
}
