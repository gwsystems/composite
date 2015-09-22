#include <stdio.h>
#include <stdlib.h>
#include "../../interface/torrent/torrent.h"
#include "../../include/cos_component.h"
#include "../../include/print.h"
#include "../musl-1.1.11/include/syscall.h"
#include <sys/uio.h>
#include <sys/mman.h>

#define SYSCALLS_NUM 378

extern void *do_mmap(size_t);
extern int do_munmap(void*, size_t);

typedef long (*cos_syscall_t)(long a, long b, long c, long d, long e, long f);
cos_syscall_t cos_syscalls[SYSCALLS_NUM];

__attribute__((regparm(1))) long
__cos_syscall(int syscall_num, long a, long b, long c, long d, long e, long f)
{
	assert(syscall_num <= SYSCALLS_NUM);

	return cos_syscalls[syscall_num](a, b, c, d, e, f);
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
	/* mode param is only for O_CREAT in flags */
	td_t td;
	long evt;
	evt = evt_split(cos_spd_id(), 0, 0);
	assert(evt > 0);
	td = tsplit(cos_spd_id(), td_root, (char *)pathname, strlen(pathname), TOR_ALL, evt);

	if (td <= 0) {
		printc("open() failed!\n");
		assert(0);
	}

	return td + 3; /* fd number adjust for reserved fd 0, 1 and 2 */
}

int
cos_close(int fd)
{
	trelease(cos_spd_id(), fd - 3); /* return void, use tor_lookup? */

	return 0; /* return -1 if failed */
}

ssize_t
cos_read(int fd, void *buf, size_t count)
{
	int ret = tread_pack(cos_spd_id(), fd - 3, buf, count);

	return ret;
}

ssize_t
cos_readv(int fd, const struct iovec *iov, int iovcnt)
{
	int i;
	ssize_t ret = 0;
	for(i=0; i<iovcnt; i++) {
		ret += cos_read(fd, (const void *)iov[i].iov_base, iov[i].iov_len);
	}
	return ret;
}

ssize_t
cos_write(int fd, const void *buf, size_t count)
{
	if (fd == 0) {
		printc("stdin is not supported!\n");
		return 0;
	} else if (fd == 1 || fd == 2) {
		int i;
		char *d = (char *)buf;
		for(i=0; i<count; i++) printc("%c", d[i]);
		return count;
	} else {
		int td = fd - 3;
		int ret = twrite_pack(cos_spd_id(), td, (char *)buf, count);
		return ret;
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
	if (fd == 1 || fd == 2) return 0;
	assert(0);
	return 0;
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
	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		assert(0);
	}
	if (fd != -1) {
		printc("file mapping is not supported!\n");
		assert(0);
	}

	void *ret = do_mmap(length);

	if (ret == (void *)-1) { /* return value comes from man page */
		printc("mmap() failed!\n");
		assert(0);
	}

	return ret;
}

int
cos_munmap(void *start, size_t length)
{
	int ret = do_munmap(start, length);
	assert(!ret);

	return ret;
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
	size_t sz = old_size;
	void *ret = do_mmap(new_size);

	if (ret == (void *)-1) { /* return value comes from man page */
		printc("mremap() failed!\n");
		assert(0);
	}
	if (new_size < sz) sz = new_size;
	memcpy(ret, old_address, sz);
	do_munmap(old_address, old_size);

	return ret;
}

off_t
cos_lseek(int fd, off_t offset, int whence)
{
	/* TODO: we can use a simpler twmeta_pack(td_t td, const char *key, const char *val) */
	char val[8]; /* TODO: length number need to be selected */
	int ret = -1;
	int td = fd - 3;

	if (whence == SEEK_SET) {
		snprintf(val, 8, "%ld", offset);
		ret = twmeta(cos_spd_id(), td, "offset", strlen("offset"), val, strlen(val));
		assert(ret == 0);
	} else if (whence == SEEK_CUR) {
		/* TODO: return value not checked */
		char offset_curr[8];
		trmeta(cos_spd_id(), td, "offset", strlen("offset"), offset_curr, 8);
		snprintf(val, 8, "%ld", atol(offset_curr) + offset);
		ret = twmeta(cos_spd_id(), td, "offset", strlen("offset"), val, strlen(val));
		assert(ret == 0);
	} else if (whence == SEEK_END) {
		printc("lseek::SEEK_END not implemented !\n");
		assert(0);
		/* TODO: how to get the length of the file? */
	}

	if   (ret != -1) return atoi(val);
	else             return ret;
}

/*int*/
/*cos_fstat(int fd, struct stat *buf)*/
/*{*/
	/*printf("syscall: fstat\n");*/
        
	/*return 0;*/
/*}*/

int
default_syscall(void)
{
	int syscall_num;
	asm volatile("movl %%eax,%0" : "=r" (syscall_num));
	printc("WARNING: Component %ld calling undifined system call %d\n", cos_spd_id(), syscall_num);

	return 0;
}

CCTOR static void
posix_init(void)
{
	int i;
	for (i = 0; i < SYSCALLS_NUM; i++) {
		cos_syscalls[i] = (cos_syscall_t)default_syscall;
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
