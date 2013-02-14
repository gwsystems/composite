#include "include/page_pool.h"
#include "include/measurement.h"
#include "include/shared/cos_types.h"
#include "include/per_cpu.h"
#include "include/chal.h"

struct page_list page_list_head;
unsigned int page_list_len = 0;

/* 
 * This does no initiation of the memory returned, do it in the
 * calling function.
 */
struct page_list *cos_get_pg_pool(void)
{
	struct page_list *page, *new_list_head;

	/*
	 * If we ran out of pages in our cache, allocate another, and
	 * copy the kernel page table mappings into it, otherwise,
	 * take a page out of our cache.
	 */
	if (NULL == page_list_head.next) {
		page = chal_alloc_page();
		if (NULL == page) return NULL;
	} else {
		do {
			page = page_list_head.next;
			new_list_head = page->next;
		} while (unlikely(!cos_cas((unsigned long *)&page_list_head.next, (unsigned long)page, (unsigned long)new_list_head)));
		page_list_len--;
		page->next = NULL;
	}

	cos_meas_event(COS_ALLOC_PGTBL);

	return page;
}

void cos_put_pg_pool(struct page_list *page)
{
	struct page_list *old_list_head;

	do {
		old_list_head = page_list_head.next;
		page->next = old_list_head;
	} while (unlikely(!cos_cas((unsigned long *)&page_list_head.next, (unsigned long)old_list_head, (unsigned long)page)));

	page_list_len++;

	/* arbitary test, but this is an error case I would like to be able to catch */
	assert(page_list_len < 1024);

	cos_meas_event(COS_FREE_PGTBL);

	return;
}

void clear_pg_pool(void)
{
	struct page_list *pg, *next;

	pg = page_list_head.next;
	while (pg) {
		next = pg->next;
		chal_free_page(pg);
		
		pg = next;
	}
}
