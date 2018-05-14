#ifndef RK_INV_H
#define RK_INV_H

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <posix.h>

#include <cos_types.h>

ssize_t rk_inv_write(int fd, const void *buf, size_t nbytes);
void *rk_inv_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int rk_inv_unlink(const char *path);
int rk_inv_ftruncate(int fd, off_t len);
ssize_t rk_inv_read(int fd, void *buf, size_t nbytes);
int rk_inv_open(const char *path, int flags, mode_t mode);
int rk_inv_clock_gettime(clockid_t clock_id, struct timespec *tp);
int rk_inv_select(int nd, fd_set *in, fd_set *ou, fd_set *ex, struct timeval *tv);
int rk_inv_socketcall(int call, unsigned long *args);

static int
rk_libcmod_init(void)
{
	posix_syscall_override((cos_syscall_t)rk_inv_socketcall, __NR_socketcall);
	posix_syscall_override((cos_syscall_t)rk_inv_mmap, __NR_mmap);
	posix_syscall_override((cos_syscall_t)rk_inv_mmap, __NR_mmap2);
	posix_syscall_override((cos_syscall_t)rk_inv_write, __NR_write);
	posix_syscall_override((cos_syscall_t)rk_inv_read, __NR_read);
	posix_syscall_override((cos_syscall_t)rk_inv_open, __NR_open);
	posix_syscall_override((cos_syscall_t)rk_inv_unlink, __NR_unlink);
	posix_syscall_override((cos_syscall_t)rk_inv_clock_gettime, __NR_clock_gettime);
	posix_syscall_override((cos_syscall_t)rk_inv_select, __NR__newselect);
	posix_syscall_override((cos_syscall_t)rk_inv_ftruncate, __NR_ftruncate64);

	return 0;
}

#endif /* RK_INV_H */
