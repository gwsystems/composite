#ifndef RK_INV_H
#define RK_INV_H

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <posix.h>
#include <fcntl.h>

#include <cos_types.h>

int rk_inv_init(void);
ssize_t rk_inv_write(int fd, const void *buf, size_t nbytes);
void *rk_inv_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int rk_inv_unlink(const char *path);
int rk_inv_ftruncate(int fd, off_t len);
ssize_t rk_inv_read(int fd, void *buf, size_t nbytes);
int rk_inv_close(int fd);
int rk_inv_open(const char *path, int flags, mode_t mode);
int rk_inv_clock_gettime(clockid_t clock_id, struct timespec *tp);
int rk_inv_select(int nd, fd_set *in, fd_set *ou, fd_set *ex, struct timeval *tv);
int rk_inv_socketcall(int call, unsigned long *args);
ssize_t rk_inv_writev(int fd, const struct iovec *iov, int iovcnt);
int rk_inv_fcntl(int fd, int cmd, void *arg);

#endif /* RK_INV_H */
