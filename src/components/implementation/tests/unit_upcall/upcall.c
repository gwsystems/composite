/**
 * Copyright 2015 by Gedare Bloom gedare@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>
#include <upcall.h>

void cos_init(void)
{
	printc("UNIT TEST upcall_invoke\n");
	upcall_invoke(cos_spd_id(), COS_UPCALL_DESTROY, cos_spd_id(), 2); 
	printc("UNIT TEST PASSED: upcall_invoke\n");
	return;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int init = 0;
	switch (t) {
	case COS_UPCALL_THD_CREATE:
		if (init == 0) {
			init = 1;
			cos_init();
			break;
		}
	default:
		printc("Upcall %d\targs (%p, %p, %p)\n", t, arg1, arg2, arg3);
		assert(t == COS_UPCALL_DESTROY);
		assert(arg1 == 2);
		break;
	}
	return;
}
