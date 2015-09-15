#include "pthread_impl.h"

int *__errno_location(void)
{
	static int e;
	return &e;
	return &__pthread_self()->errno_val;
}
