#ifndef COS_ALLOC_H
#define COS_ALLOC_H

//#define ALLOC_DEBUG ALLOC_DEBUG_ALL

#include <stddef.h>

void *malloc(size_t sz);
void  free(void *addr);
void  free_page(void *ptr);
void *alloc_page(void);

/* ...not to be used without mem_mgr_large */
void *page_alloc(int npages);
void  page_free(void *ptr, int npages);

#define ALLOC_DEBUG_STATS 1
#define ALLOC_DEBUG_ALL 2
//#define ALLOC_DEBUG ALLOC_DEBUG_STATS
#ifndef ALLOC_DEBUG
#define alloc_stats_print()
#else
void alloc_stats_print(void);
#endif

#endif
