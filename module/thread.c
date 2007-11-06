/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

//#include <thread.h>
//#include <spd.h>
#include "include/thread.h"
#include "include/spd.h"

struct thread threads[MAX_NUM_THREADS];
static struct thread *thread_freelist_head = MNULL;
/* like "current" in linux */
struct thread *current_thread;

void thd_init_all(struct thread *thds)
{
	int i;

	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		/* adjust the thread id to avoid using thread 0 clear */
		thds[i].thread_id = i+1;
		thds[i].freelist_next = (i == (MAX_NUM_THREADS-1)) ? MNULL : &thds[i+1];
	}

	thread_freelist_head = thds;

	return;
}

struct thread *thd_alloc(struct spd *spd)
{
	struct thread *thd;

	thd = thread_freelist_head;

	if (thd == MNULL) {
		printk("Could not create thread.\n");
		return MNULL;
	}

	thread_freelist_head = thread_freelist_head->freelist_next;

	/* Initialization */
	thd->stack_ptr = -1;
	/* establish this thread's base spd */
	thd_invocation_push(thd, spd, 0, 0);

	thd->sched_suspended = NULL;
	thd->flags = 0;

	thd->thread_brand = NULL;
	thd->pending_upcall_requests = 0;
	thd->brand_inv_stack_ptr = 0;

	return thd;
}


void thd_free(struct thread *thd)
{
	thd->freelist_next = thread_freelist_head;
	thread_freelist_head = thd;

	return;
}

void thd_init(void)
{
	thd_init_all(threads);
	current_thread = NULL;
}
