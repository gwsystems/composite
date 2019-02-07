#ifndef RK_LIBC_OVERRIDE_H
#define RK_LIBC_OVERRIDE_H

#include <posix.h>
#include <rk_inv.h>

static int
rk_libcmod_init(void)
{
	posix_syscall_override((cos_syscall_t)rk_inv_socketcall, __NR_socketcall);
	posix_syscall_override((cos_syscall_t)rk_inv_mmap, __NR_mmap);
	posix_syscall_override((cos_syscall_t)rk_inv_mmap, __NR_mmap2);
	posix_syscall_override((cos_syscall_t)rk_inv_write, __NR_write);
	posix_syscall_override((cos_syscall_t)rk_inv_read, __NR_read);
	posix_syscall_override((cos_syscall_t)rk_inv_open, __NR_open);
	posix_syscall_override((cos_syscall_t)rk_inv_close, __NR_close);
	posix_syscall_override((cos_syscall_t)rk_inv_unlink, __NR_unlink);
	posix_syscall_override((cos_syscall_t)rk_inv_clock_gettime, __NR_clock_gettime);
	posix_syscall_override((cos_syscall_t)rk_inv_select, __NR__newselect);
	posix_syscall_override((cos_syscall_t)rk_inv_ftruncate, __NR_ftruncate64);

	rk_inv_init();

	return 0;
}

#endif
