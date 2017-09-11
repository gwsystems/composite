#include <assert.h>

extern "C" void _Unwind_DeleteException (void *a)
{
	assert(0);
}
