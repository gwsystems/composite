#include <errno.h>
#include "pthread_impl.h"

int *__errno_location(void)
{
	static int e;
	return &e;
}

weak_alias(__errno_location, ___errno_location);
