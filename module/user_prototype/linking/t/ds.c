#include <cos_component.h>
#include <cos_scheduler.h>

extern int sched_create_child_brand(void);
extern void sched_child_yield_thd(void);

int upcall_id;
volatile int ticks = 0;

static void timer_tick(int amnt)
{
//	ticks += amnt;
//	print("ds upcall thd %d with tick %d (amnt %d).", upcall_id, ticks, amnt);

	return;
}

static void sched_init(void)
{
	print("ds sched_init %d%d%d", 0,0,0);
	upcall_id = sched_create_child_brand();
	print("created ds upcall %d. %d%d", upcall_id, 0,0);
	sched_child_yield_thd();
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		static int first = 1;
		if (first) {
			cos_argreg_init();
			first = 0;
		}
		timer_tick((int)arg1);
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
		sched_init();
		break;
	case COS_UPCALL_CREATE:
		assert(0);
		cos_argreg_init();
//		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		assert(0);
		break;
	default:
		assert(0);
		return;
	}

	return;
}

