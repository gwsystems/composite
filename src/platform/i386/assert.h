#include "kernel.h"

#undef assert
//#ifndef NDEBUG
#define assert(x) do { \
	if (0 == (x)) { \
	die("Assertion '%s' failed at %s:%d in function %s\n", #x, __FILE__, __LINE__, __func__); \
} } while (0)
/* #else
#define assert(x) ((void)0)
#endif */
