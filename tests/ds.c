#include <cos_component.h>
#include <cos_scheduler.h>

static void sched_init(void)
{
	
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		static int first = 1;

		if (sched_get_current() != timer) {
			print("sched_get_current %d, timer %d. %d", (unsigned int)cos_get_thd_id(), (unsigned int)timer->id, 1);
		}
		assert(sched_get_current() == timer);
		if (first) {
			cos_argreg_init();
			first = 0;
		}
		fp_timer_tick();
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
		sched_init();
		break;
	case COS_UPCALL_CREATE:
		cos_argreg_init();
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		fp_event_completion(sched_get_current());
		break;
	default:
		assert(0);
		return;
	}

	return;
}

