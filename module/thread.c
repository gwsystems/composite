/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#include <thread.h>
//#include <spd.h>
#include "include/thread.h"
#include "include/spd.h"
#include "include/page_pool.h"

struct thread threads[MAX_NUM_THREADS];
static struct thread *thread_freelist_head = NULL;
/* like "current" in linux */
struct thread *current_thread = NULL;

int thd_spd_in_current_composite(struct thread *thd, struct spd *spd)
{
	struct spd_poly *composite = thd_get_thd_spdpoly(thd);

	/*
	 * First we check the cache: spd->composite_spd which will
	 * point to the composite spd that the spd is currently part
	 * of.  It is very likely that we will hit in this cache
	 * unless we are using a stale mapping.  In such a case, we
	 * must look in the page table of the composite and see if the
	 * spd is present.  This is significantly more expensive.
	 */
	return spd->composite_spd == composite || 
		spd_composite_member(spd, composite);
}

void thd_init_all(struct thread *thds)
{
	int i;

	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		/* adjust the thread id to avoid using thread 0 clear */
		thds[i].thread_id = i+1;
		thds[i].freelist_next = (i == (MAX_NUM_THREADS-1)) ? NULL : &thds[i+1];
	}

	thread_freelist_head = thds;

	return;
}

extern void *va_to_pa(void *va);
extern void thd_publish_data_page(struct thread *thd, vaddr_t page);

struct thread *thd_alloc(struct spd *spd)
{
	struct thread *thd;
	unsigned short int id;
	void *page;

	thd = thread_freelist_head;
	if (thd == NULL) {
		printk("cos: Could not create thread.\n");
		return NULL;
	}
	
	page = cos_get_pg_pool();
	if (NULL == page) {
		printk("cos: Could not allocate the data page for new thread.\n");
		
	}
	thread_freelist_head = thread_freelist_head->freelist_next;

	id = thd->thread_id;
	memset(thd, 0, sizeof(struct thread));
	thd->thread_id = id;

	thd->data_region = page;
	thd->ul_data_page = COS_INFO_REGION_ADDR + (PAGE_SIZE * id);
	thd_publish_data_page(thd, (vaddr_t)page);

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
	if (NULL == thd) return;

	while (thd->stack_ptr > 0) {
		struct thd_invocation_frame *frame;

		/*
		 * FIXME: this should include upcalling into effected
		 * spds, to inform them of the deallocation.
		 */

		frame = &thd->stack_base[thd->stack_ptr];
		spd_mpd_ipc_release((struct composite_spd*)frame->current_composite_spd);

		thd->stack_ptr--;
	}

	if (NULL != thd->data_region) {
		cos_put_pg_pool((struct page_list*)thd->data_region);
	}

	thd->freelist_next = thread_freelist_head;
	thread_freelist_head = thd;

	return;
}

void thd_init(void)
{
	thd_init_all(threads);
	current_thread = NULL;
}
