/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/thread.h"
#include "include/spd.h"
#include "include/page_pool.h"

struct thread threads[MAX_NUM_THREADS];
static struct thread *thread_freelist_head = NULL;

/* 
 * Return the depth into the stack were we are present or -1 for
 * error/not present.
 */
int thd_validate_spd_in_callpath(struct thread *t, struct spd *s)
{
	int i;

	assert(t->stack_ptr >= 0);

	for (i = t->stack_ptr ; i >= 0 ; i--) {
		struct thd_invocation_frame *f;

		f = &t->stack_base[i];
		assert(f && f->current_composite_spd);
		if (thd_spd_in_composite(f->current_composite_spd, s)) {
			return t->stack_ptr - i;
		}
	}
	return -1;
}

void thd_init_all(struct thread *thds)
{
	int i;

	memset(threads, 0, sizeof(struct thread) * MAX_NUM_THREADS);
	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		/* adjust the thread id to avoid using thread 0 clear */
		thds[i].thread_id = i+1;
		thds[i].freelist_next = (i == (MAX_NUM_THREADS-1)) ? NULL : &thds[i+1];
	}

	thread_freelist_head = thds;

	return;
}

extern void thd_publish_data_page(struct thread *thd, vaddr_t page);

struct thread *thd_alloc(struct spd *spd)
{
	struct thread *thd, *new_freelist_head;
	unsigned short int id;
	void *page;

	do {
		thd = thread_freelist_head;
		new_freelist_head = thread_freelist_head->freelist_next;
	} while (unlikely(!cos_cas((unsigned long *)&thread_freelist_head, (unsigned long)thd, (unsigned long)new_freelist_head)));

	if (thd == NULL) {
		printk("cos: Could not create thread.\n");
		return NULL;
	}

	page = cos_get_pg_pool();
	if (unlikely(NULL == page)) {
		printk("cos: Could not allocate the data page for new thread.\n");
		thread_freelist_head = thd;
		return NULL;
	}

	id = thd->thread_id;
	memset(thd, 0, sizeof(struct thread));
	thd->thread_id = id;

	thd->data_region = page;
	*(int*)page = 4; /* HACK: sizeof(struct cos_argr_placekeeper) */
	thd->ul_data_page = COS_INFO_REGION_ADDR + (PAGE_SIZE * id);
	thd_publish_data_page(thd, (vaddr_t)page);

	/* Initialization */
	thd->stack_ptr = -1;
	/* establish this thread's base spd */
	thd_invocation_push(thd, spd, 0, 0);

	thd->flags = 0;

	thd->thread_brand = NULL;
	thd->pending_upcall_requests = 0;
	thd->freelist_next = NULL;

	thd->fpu.status = 0;
	thd->fpu.saved_fpu = 0;

	return thd;
}


void thd_free(struct thread *thd)
{
	struct thread *old_freelist_head;
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

	do {
		old_freelist_head = thread_freelist_head;
		thd->freelist_next = old_freelist_head;
	} while (unlikely(!cos_cas((unsigned long *)&thread_freelist_head, (unsigned long)old_freelist_head, (unsigned long)thd)));

	return;
}

void thd_free_all(void)
{
	struct thread *t;
	int i;

	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		t = &threads[i];

		/* is the thread active (not free)? */
		if (t->freelist_next == NULL) {
			thd_free(t);
		}
	}
}

void thd_init(void)
{
	thd_init_all(threads);
	/* current_thread = NULL; // Not used anymore */
}

extern int host_in_syscall(void);
extern int host_in_idle(void);
/*
 * Is the thread currently in an atomic section?  If so, rollback its
 * instruction pointer to the beginning of the section (the commit has
 * not yet happened).
 */
int thd_check_atomic_preempt(struct thread *thd)
{
	struct spd *spd = thd_get_thd_spd(thd);
	vaddr_t ip = thd_get_ip(thd);
	int i;
	
	assert(host_in_syscall() || host_in_idle() || 
	       thd->flags & THD_STATE_PREEMPTED);

	for (i = 0 ; i < COS_NUM_ATOMIC_SECTIONS/2 ; i+=2) {
		if (ip > spd->atomic_sections[i] && 
		    ip < spd->atomic_sections[i+1]) {
			thd->regs.ip = spd->atomic_sections[i];
			cos_meas_event(COS_MEAS_ATOMIC_RBK);
			return 1;
		}
	}
	
	return 0;
}

void thd_print_regs(struct thread *t) {
	struct pt_regs *r = &t->regs;
	struct spd *s = thd_get_thd_spd(t);

	printk("cos: spd %d, thd %d w/ regs: \ncos:\t\t"
	       "eip %10x, esp %10x, eax %10x, ebx %10x, ecx %10x,\ncos:\t\t"
	       "edx %10x, edi %10x, esi %10x, ebp %10x \n",
	       spd_get_index(s), thd_get_id(t), (unsigned int)r->ip, (unsigned int)r->sp, 
	       (unsigned int)r->ax, (unsigned int)r->bx, (unsigned int)r->cx, (unsigned int)r->dx, 
	       (unsigned int)r->di, (unsigned int)r->si, (unsigned int)r->bp);

	return;
}
