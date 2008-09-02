#ifndef COS_ALLOC_H
#define COS_ALLOC_H

#include <stddef.h>

void *malloc(size_t sz);
void free(void *addr);
void free_page(void *ptr);
void *alloc_page(void);

#endif
