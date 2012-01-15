#include <cos_component.h>
#include <print.h>
#include <sched.h>

#define ITER 256
int first = 0, other = 0;

static void cos_create_thd(char *prio)
{
	struct cos_array *data;

	data = cos_argreg_alloc(sizeof(struct cos_array) + strlen(prio) + 1);
	assert(data);
	strcpy(&data->mem[0], prio);
	data->sz = strlen(prio)+1;
	if (0 > (other = sched_create_thread(cos_spd_id(), data))) BUG();
	cos_argreg_free(data);
}

void cos_init(void)
{
	int i;
	if (cos_get_thd_id() == other) {
		for (i = 0 ; i < ITER ; i++) {
			sched_wakeup(cos_spd_id(), first);
			sched_block(cos_spd_id(), 0);
		}
	} else {
		first = cos_get_thd_id();
		cos_create_thd("a11");
		for (i = 0 ; i < ITER ; i++) {
			sched_block(cos_spd_id(), 0);
			sched_wakeup(cos_spd_id(), other);
		}
	}
}
