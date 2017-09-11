#include <errno.h>
#include <stdarg.h>
#include <syscall.h>
#include "pthread_impl.h"

int __clone(int (*a)(void *), void *b, int c, void *d, void* e, void* f, void* g)
{
	return (int) __cos_syscall(__NR_clone, (long)a, (long)b, (long)c, (long)d, (long)e, (long)f, (long)g);
}
