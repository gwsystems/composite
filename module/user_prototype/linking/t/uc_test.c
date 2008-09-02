#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

extern void sched_create_brand(void);

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	unsigned long long end;
	static int cnt = 0;
	static unsigned long avg = 0;
	unsigned long diff;

	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
		/* return immediately */
//		print("brand testing upcalls!  %d%d%d", 0,0,0);
		rdtscll(end);
		diff = end-(unsigned int)arg2;
//		print("brand upcall invocation made @ %u, end @ %u, diff %u.", (unsigned int)arg2, end, diff);
		cnt++;
		if (avg == 0) {
			avg = diff;
		} else {
			avg = (0.9 * avg) + (0.1 * diff);
		}
		if (cnt == 99999) {
			print("avg response time is %u. %d%d", avg, 0,0);
		}
		
		break;
	case COS_UPCALL_BOOTSTRAP:
	{
//		print("bootstrapping upcall test component %d%d%d", 0,0,0);
		sched_create_brand();
		break;
	}
//	case COS_UPCALL_CREATE:
//		break;
//	case COS_UPCALL_BRAND_COMPLETE:
//		assert(0);
//		break;
	default:
		print("wtf mate, unknown upcall option. %d%d%d", 0,0,0);
		assert(0);
		return;
	}

	return;
}

