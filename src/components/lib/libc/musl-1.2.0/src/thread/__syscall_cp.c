#include "pthread_impl.h"
#include "syscall.h"

#ifdef SHARED
hidden
#endif
long __syscall_cp_c();

static long sccp(syscall_arg_t nr,
                 syscall_arg_t u, syscall_arg_t v, syscall_arg_t w,
                 syscall_arg_t x, syscall_arg_t y, syscall_arg_t z)
{
	return __cos_syscall(nr, u, v, w, x, y, z, 0);
}

weak_alias(sccp, __syscall_cp_c);

long (__syscall_cp)(syscall_arg_t nr,
                    syscall_arg_t u, syscall_arg_t v, syscall_arg_t w,
                    syscall_arg_t x, syscall_arg_t y, syscall_arg_t z)
{
	return __syscall_cp_c(nr, u, v, w, x, y, z);
}
