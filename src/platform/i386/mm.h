#ifndef _MM_H_
#define _MM_H_

#include "types.h"

void *kmalloc_a(size_t size);
void *kmalloc_p(size_t size, uintptr_t *phys);
void *kmalloc_ap(size_t size, uintptr_t *phys);
void *kmalloc(size_t size);

#endif
