#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>

volatile int* null_ptr = NULL;
#define ABORT() do {int i = *null_ptr;} while(0)

#define SYSCALLS_NUM 378

typedef long (*cos_syscall_t)(long a, long b, long c, long d, long e, long f);
cos_syscall_t cos_syscalls[SYSCALLS_NUM];

static void
libc_syscall_override(cos_syscall_t fn, int syscall_num)
{
	printc("Overriding syscall %d\n", syscall_num);
	cos_syscalls[syscall_num] = fn;
}

ssize_t
cos_write(int fd, const void *buf, size_t count)
{
	// You shouldn't write to stdin anyway, so don't bother special casing it
	if (fd == 1 || fd == 2) {
		size_t i;
		char *d = (char *)buf;
		for(i = 0; i < count; i++) printc("%c", d[i]);
		return count;
	} else {
		printc("fd: %d not supported!\n", fd);
		assert(0);
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
cos_ioctl(int fd, int request, void *data)
{
	/* musl libc does some ioctls to stdout, so just allow these to silently go through */
	if (fd == 1 || fd == 2) return 0;

	printc("ioctl on fd(%d) not implemented\n", fd);
	assert(0);
	return 0;
}

ssize_t
cos_brk(void *addr)
{
	printc("brk not implemented\n");
	// I'm unsure if the below comment is accurate. We return 0, so doesn't brk "succeed"?
	/* musl libc tries to use brk to expand heap in malloc. But if brk fails, it
	   turns to mmap. So this fake brk always fails, force musl libc to use mmap */
	return 0;
}

void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret=0;

	printc("mmap\n");
	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}
	if (fd != -1) {
		printc("file mapping is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	addr = (void *)cos_page_bump_allocn(&cos_defcompinfo_curr_get()->ci, length);
	if (!addr){
		ret = (void *) -1;
	} else {
		ret = addr;
	}

	if (ret == (void *)-1) {  //return value comes from man page
		printc("mmap() failed!\n");
		// This is a best guess about what went wrong
		errno = ENOMEM;
	}
	return ret;
}

int
cos_munmap(void *start, size_t length)
{
	printc("munmap not implemented\n");
	errno = ENOSYS;
	return -1;
}

int
cos_madvise(void *start, size_t length, int advice)
{
	/* We don't do anything with the advice from madvise, but that isn't really a problem */
	return 0;
}

void *
cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
	printc("mremap not implemented\n");
	errno = ENOSYS;
	return (void*) -1;
}

int
cos_rt_sigprocmask(int how, void* set, void* oldset, size_t sigsetsize)
{
	// Musl uses this, so we let it go through silently
	printc("rt_sigprocmask not implemented\n");
	return -ENOSYS;
}

// Thread related functions
pid_t
cos_gettid(void)
{
	return (pid_t) sl_thdid();
}

int
cos_tkill(int tid, int sig)
{
	if (sig == SIGABRT || sig == SIGKILL) {
		printc("Abort requested, complying...\n");
		ABORT();
	} else if (sig == SIGFPE) {
		printc("Floating point error, aborting...\n");
		ABORT();
	} else if (sig == SIGILL) {
		printc("Illegal instruction, aborting...\n");
		ABORT();
	} else if (sig == SIGINT) {
		printc("Terminal interrupt, aborting?\n");
		ABORT();
	} else {
		printc("Unknown signal %d\n", sig);
		assert(0);
	}

	assert(0);
	return 0;
}

struct user_desc {
    unsigned int  entry_number; // Ignore
    unsigned int  base_addr; // Pass to cos thread mod
    unsigned int  limit; // Ignore
    unsigned int  seg_32bit:1;
    unsigned int  contents:2;
    unsigned int  read_exec_only:1;
    unsigned int  limit_in_pages:1;
    unsigned int  seg_not_present:1;
    unsigned int  useable:1;
};

// void* backing_data[SL_MAX_NUM_THDS];

int
cos_set_thread_area(void* data)
{
	printc("cos_set_thread_area %p\n", data);
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	thdid_t thdid = sl_thdid();

	cos_thd_mod(ci, sl_thd_curr()->thdcap, data);
	return 0;
}

long
cos_set_tid_address(int *tidptr)
{
	// Just do nothing for now and hope that works
	return 0;
}


void
pre_syscall_default_setup()
{
	printc("pre_syscall_default_setup\n");

	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	cos_defcompinfo_init();
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
}

void
syscall_emulation_setup(void)
{
	printc("syscall_emulation_setup\n");

	int i;
	for (i = 0; i < SYSCALLS_NUM; i++) {
		cos_syscalls[i] = 0;
	}

	libc_syscall_override((cos_syscall_t)cos_write, __NR_write);
	libc_syscall_override((cos_syscall_t)cos_writev, __NR_writev);
	libc_syscall_override((cos_syscall_t)cos_ioctl, __NR_ioctl);
	libc_syscall_override((cos_syscall_t)cos_brk, __NR_brk);
	libc_syscall_override((cos_syscall_t)cos_mmap, __NR_mmap);
	libc_syscall_override((cos_syscall_t)cos_mmap, __NR_mmap2);
	libc_syscall_override((cos_syscall_t)cos_munmap, __NR_munmap);
	libc_syscall_override((cos_syscall_t)cos_madvise, __NR_madvise);
	libc_syscall_override((cos_syscall_t)cos_mremap, __NR_mremap);

	libc_syscall_override((cos_syscall_t)cos_rt_sigprocmask, __NR_rt_sigprocmask);
	libc_syscall_override((cos_syscall_t)cos_gettid, __NR_gettid);
	libc_syscall_override((cos_syscall_t)cos_tkill, __NR_tkill);
	libc_syscall_override((cos_syscall_t)cos_set_thread_area, __NR_set_thread_area);
	libc_syscall_override((cos_syscall_t)cos_set_tid_address, __NR_set_tid_address);
}

long
cos_syscall_handler(int syscall_num, long a, long b, long c, long d, long e, long f)
{
	assert(syscall_num <= SYSCALLS_NUM);
	printc("Making syscall %d\n", syscall_num);

	if (!cos_syscalls[syscall_num]){
		printc("WARNING: Component %ld calling unimplemented system call %d\n", cos_spd_id(), syscall_num);
		assert(0);
		return 0;
	} else {
		return cos_syscalls[syscall_num](a, b, c, d, e, f);
	}
}

void
libc_initialization_handler()
{
	printc("libc_init\n");
	libc_init();
}
