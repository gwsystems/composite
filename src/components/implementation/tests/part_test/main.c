#include <part.h>
#include <part_task.h>

#define NTHDS 2

void
work_fn(void *d)
{
	PRINTC("Sharing work!\n");
}

int
main(void)
{
	struct sl_thd *c = sl_thd_curr();
	struct part_task *p = (struct part_task *)c->part_context, *pt = &main_task;
	int n = NTHDS > PART_MAX_PAR_THDS ? PART_MAX_PAR_THDS : NTHDS;

	assert(p == NULL);
	
	pt->state = PART_TASK_S_ALLOCATED;
	part_task_init(pt, PART_TASK_T_WORKSHARE, p, n, work_fn, NULL, NULL);
	assert(pt->nthds = n);

	c->part_context = pt;
	part_list_append(pt);

	work_fn(NULL);
	part_task_end(pt);

	PRINTC("Done!\n");
}
