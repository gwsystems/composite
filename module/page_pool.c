#include "include/page_pool.h"
#include "include/measurement.h"
#include "include/cos_types.h"

extern void *cos_alloc_page(void);
extern void *cos_free_page(void *page);
extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);

struct page_list page_list_head;
unsigned int page_list_len = 0;

/* 
 * This does no initiation of the memory returned, do it in the
 * calling function.
 */
struct page_list *cos_get_pg_pool(void)
{
	struct page_list *page;

	/*
	 * If we ran out of pages in our cache, allocate another, and
	 * copy the kernel page table mappings into it, otherwise,
	 * take a page out of our cache.
	 */
	if (NULL == page_list_head.next) {
		page = cos_alloc_page();
		if (NULL == page) return NULL;
	} else {
		page = page_list_head.next;
		page_list_head.next = page->next;
		page_list_len--;
		page->next = NULL;
	}

	cos_meas_event(COS_ALLOC_PGTBL);

	return page;
}

void cos_put_pg_pool(struct page_list *page)
{
	page->next = page_list_head.next;
	page_list_head.next = page;
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
		cos_free_page(pg);
		
		pg = next;
	}
}
