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

#define _PART_PRIO 1
#define _PART_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_PRIO)

unsigned
part_isready(void)
{ return part_ready; }

void
part_init(void)
{
	int j;
	struct sl_xcore_thd *x;
	sched_param_t p = _PART_PRIO_PACK();
	sched_param_t pa[1] = { p };
	struct sl_thd *t;
	static int is_first = NUM_CPU;

	ps_list_head_init(&part_l_global);
	if (!ps_cas((unsigned long *)&is_first, NUM_CPU, cos_cpuid())) return;
	
	for (j = 0; j < NUM_CPU; j++) {
		int k;

		if (j == cos_cpuid()) {
			for (k = 0; k < PART_MAX_CORE_THDS; k++) {
				t = sl_thd_alloc(part_thd_fn, NULL);
				assert(t);

				sl_thd_param_set(t, p);

				x = sl_xcore_thd_lookup(sl_thd_thdid(t), cos_cpuid());
				assert(x);
			}

		} else {
			for (k = 0; k < PART_MAX_CORE_THDS; k++) {
				x = sl_xcore_thd_alloc(j, part_thd_fn, NULL, 1, pa);
				assert(x);
			}
		}
	}

	part_ready = 1;
}
