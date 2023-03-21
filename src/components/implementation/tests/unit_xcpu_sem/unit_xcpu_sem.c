#include <llprint.h>
#include <sched.h>

#include <sync_sem.h>

static struct sync_sem sem;

void
cos_init()
{
	assert(NUM_CPU == 2);
	sync_sem_init(&sem, 0);
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{

	if (cid == 0) {
		while (1)
		{
			printc("G\n");
			sync_sem_give(&sem);
		}
		

	} else {
		while (1)
		{
			// might block and never wake up
			printc("T\n");
			sync_sem_take(&sem);
		}
	}
	return;
}