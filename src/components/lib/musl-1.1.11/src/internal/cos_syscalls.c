#include <stdlib.h>
	
__attribute__((weak, regparm(1))) long
__cos_syscall(int syscall_num, long a, long b, long c, long d, long e, long f, long g)
{
        long *x = NULL;
        volatile long y = *x;

        return y;
}
