#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <syscall.h>
#include <stdbool.h>

#define SYSCALLS_NUM 378

extern struct cos_compinfo parent_cinfo;

typedef long (*cos_syscall_t)(long a, long b, long c, long d, long e, long f);
cos_syscall_t cos_syscalls[SYSCALLS_NUM];

__attribute__((regparm(1))) long
__cos_syscall(int syscall_num, long a, long b, long c, long d, long e, long f)
{
	assert(syscall_num <= SYSCALLS_NUM);

	if (!cos_syscalls[syscall_num]) {
		printc("WARNING: Component %ld calling unimplemented system call %d\n", cos_spd_id(), syscall_num);
		return 0;
	} else {
		return cos_syscalls[syscall_num](a, b, c, d, e, f);
	}
}

static void
libc_syscall_override(cos_syscall_t fn, int syscall_num)
{
	cos_syscalls[syscall_num] = fn;

	return;
}

int
cos_open(const char *pathname, int flags, int mode)
{
	printc("open not implemented!\n");
	return -ENOTSUP;
}

int
cos_close(int fd)
{
	printc("close not implemented!\n");
	return -ENOTSUP;
}

ssize_t
cos_read(int fd, void *buf, size_t count)
{
	printc("read not implemented!\n");
	return -ENOTSUP;
}

ssize_t
cos_readv(int fd, const struct iovec *iov, int iovcnt)
{
	printc("readv not implemented!\n");
	return -ENOTSUP;
}

ssize_t
cos_write(int fd, const void *buf, size_t count)
{
	if (fd == 0) {
		printc("stdin is not supported!\n");
		return -ENOTSUP;
	} else if (fd == 1 || fd == 2) {
		int i;
		char *d = (char *)buf;
		for(i=0; i<count; i++) printc("%c", d[i]);
		return count;
	} else {
		printc("write not implemented!\n");
		return -ENOTSUP;
	}
}

ssize_t
cos_writev(int fd, const struct iovec *iov, int iovcnt)
{
	int i;
	ssize_t ret = 0;
	for(i=0; i<iovcnt; i++) {
		ret += cos_write(fd, (const void *)iov[i].iov_base, iov[i].iov_len);
	}
	return ret;
}

long
cos_ioctl(int fd, void *unuse1, void *unuse2)
{
	/* musl libc does some ioctls to stdout, so just allow these to silently go through */
	if (fd == 1 || fd == 2) {
		return 0;
	} else {
		printc("ioctl not implemented!\n");
		return -ENOTSUP;
	}
}

ssize_t
cos_brk(void *addr)
{
	/* musl libc tries to use brk to expand heap in malloc. But if brk fails, it 
	   turns to mmap. So this fake brk always fails, force musl libc to use mmap */
	return 0;
}

void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret = 0;

	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		assert(0);
	}
	if (fd != -1) {
		printc("file mapping is not supported!\n");
		assert(0);
	}

	addr = (void *)cos_page_bump_allocn(&parent_cinfo, length);
	if (!addr) {
		/*the return value comes from man page*/
		ret = (void *)-1;
	} else {
		ret = addr;
	}

	return ret;
}

int
cos_munmap(void *start, size_t length)
{
	printc("munmap not implemented!\n");
	return -ENOTSUP;
}

int
cos_madvise(void *start, size_t length, int advice)
{
	/* musl libc use madvise in free. Again allow these to silently go through */
	return 0;
}

void *
cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
	printc("mremap not implemented!\n");
	return -ENOTSUP;
}

off_t
cos_lseek(int fd, off_t offset, int whence)
{
	printc("lseek not implemented!\n");
	return -ENOTSUP;
}

CCTOR static void
posix_init(void)
{
	int i;
	for (i = 0; i < SYSCALLS_NUM; i++) {
		cos_syscalls[i] = 0;
	}

	libc_syscall_override((cos_syscall_t)cos_open, __NR_open);
	libc_syscall_override((cos_syscall_t)cos_close, __NR_close);
	libc_syscall_override((cos_syscall_t)cos_read, __NR_read);
	libc_syscall_override((cos_syscall_t)cos_readv, __NR_readv);
	libc_syscall_override((cos_syscall_t)cos_write, __NR_write);
	libc_syscall_override((cos_syscall_t)cos_writev, __NR_writev);
	libc_syscall_override((cos_syscall_t)cos_ioctl, __NR_ioctl);
	libc_syscall_override((cos_syscall_t)cos_brk, __NR_brk);
	libc_syscall_override((cos_syscall_t)cos_mmap, __NR_mmap);
	libc_syscall_override((cos_syscall_t)cos_mmap, __NR_mmap2);
	libc_syscall_override((cos_syscall_t)cos_munmap, __NR_munmap);
	libc_syscall_override((cos_syscall_t)cos_madvise, __NR_madvise);
	libc_syscall_override((cos_syscall_t)cos_mremap, __NR_mremap);
	libc_syscall_override((cos_syscall_t)cos_lseek, __NR_lseek);

	return;
}
