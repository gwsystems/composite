#ifndef PAGE_POOL_H
#define PAGE_POOL_H

struct page_list {
	struct page_list *next;
};

void clear_pg_pool(void);
void cos_put_pg_pool(struct page_list *page);
struct page_list *cos_get_pg_pool(void);

#endif
