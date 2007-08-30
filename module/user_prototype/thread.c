/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#include <thread.h>
#include <spd.h>

static struct thread threads[MAX_NUM_THREADS];
static struct thread *thread_freelist_head = MNULL;
/* like "current" in linux */
struct thread *current_thread;


void thd_init_all(struct thread *thds)
{
	int i;
	unsigned short int id = 0;

	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		thds[i].thread_id = id;
		thds[i].freelist_next = (i == (MAX_NUM_THREADS-1)) ? MNULL : &thds[i+1];
		id++;
	}

	thread_freelist_head = thds;

	return;
}

struct thread *thd_alloc(struct spd *spd)
{
	struct thread *thd;

	thd = thread_freelist_head;

	if (thd == MNULL)
		return MNULL;

	thread_freelist_head = thread_freelist_head->freelist_next;

	/* Initialization */
	thd->stack_ptr = -1;
	/* establish this thread's base spd */
	thd_invocation_push(thd, (struct composite_spd*)spd, 0, 0, 0);
		
	return thd;
}


void thd_free(struct thread *thd)
{
	thd->freelist_next = thread_freelist_head;
	thread_freelist_head = thd;

	return;
}

void thd_set_current(struct thread *thd)
{
	current_thread = thd;

	return;
}

void thd_init(void)
{
	thd_init_all(threads);
}

