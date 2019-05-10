#include <cos_types.h>
#include <cos_component.h>
#include <part_task.h>
#include <part.h>

#include <sl.h>
#include <sl_xcore.h>

struct deque_part part_dq_percore[NUM_CPU];
//struct cirque_par parcq_global;
struct ps_list_head part_l_global;
static unsigned part_ready = 0;
struct crt_lock part_l_lock;

#define _PART_PRIO 1
#define _PART_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_PRIO)

unsigned
part_isready(void)
{ return (part_ready == NUM_CPU); }

void
part_init(void)
{
	int k;
	static int is_first = NUM_CPU, ds_init_done = 0;

	if (!ps_cas(&is_first, NUM_CPU, cos_cpuid())) {
		while (!ps_load(&ds_init_done)) ;
	} else {
		ps_list_head_init(&part_l_global);
		crt_lock_init(&part_l_lock);
		ps_faa(&ds_init_done, 1);
	}
	
	for (k = 0; k < PART_MAX_CORE_THDS; k++) {
		struct sl_xcore_thd *x;
		struct sl_thd *t;
		sched_param_t p = _PART_PRIO_PACK();

		t = sl_thd_alloc(part_thd_fn, NULL);
		assert(t);

		sl_thd_param_set(t, p);

		x = sl_xcore_thd_lookup_init(sl_thd_thdid(t), cos_cpuid());
		assert(x);
	}

	ps_faa(&part_ready, 1);
}
