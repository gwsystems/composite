#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <posix.h>
#include <ps_list.h>
#include <sched.h>
#include <memmgr.h>

volatile int* null_ptr = NULL;
#define ABORT() do {int i = *null_ptr;} while(0)

extern cos_syscall_t cos_syscalls[SYSCALLS_NUM];

struct ps_lock stdout_lock;

ssize_t
write_bytes_to_stdout(const char *buf, size_t count)
{
	size_t i;
	for(i = 0; i < count; i++) printc("%c", buf[i]);
	return count;
}

ssize_t
cos_write(int fd, const void *buf, size_t count)
{
	/* You shouldn't write to stdin anyway, so don't bother special casing it */
	if (fd == 1 || fd == 2) {
		ps_lock_take(&stdout_lock);
		write_bytes_to_stdout((const char *) buf, count);
		ps_lock_release(&stdout_lock);
		return count;
	} else {
		printc("fd: %d not supported!\n", fd);
		assert(0);
	}
}

ssize_t
cos_writev(int fd, const struct iovec *iov, int iovcnt)
{
	if (fd == 1 || fd == 2) {
		ps_lock_take(&stdout_lock);
		int i;
		ssize_t ret = 0;
		for(i=0; i<iovcnt; i++) {
			ret += write_bytes_to_stdout((const void *)iov[i].iov_base, iov[i].iov_len);
		}
		ps_lock_release(&stdout_lock);
		return ret;
	} else {
		printc("fd: %d not supported!\n", fd);
		assert(0);
	}
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
	/* Unsure if the below comment is accurate. We return 0, so doesn't brk "succeed"? */
	/* musl libc tries to use brk to expand heap in malloc. But if brk fails, it
	   turns to mmap. So this fake brk always fails, force musl libc to use mmap */
	return 0;
}

void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret=0;

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

	addr = (void *)memmgr_heap_page_allocn((length / PAGE_SIZE));
	if (!addr){
		ret = (void *) -1;
	} else {
		ret = addr;
	}

	if (ret == (void *)-1) {  /* return value comes from man page */
		printc("mmap() failed!\n");
		/* This is a best guess about what went wrong */
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
	/* Musl uses this at thread create time */
	printc("rt_sigprocmask not implemented\n");
	errno = ENOSYS;
	return -1;
}

int
cos_mprotect(void *addr, size_t len, int prot)
{
	/* Musl uses this at thread create time */
	printc("mprotect not implemented\n");
	return 0;
}

/* Thread related functions */
pid_t
cos_gettid(void)
{
	return (pid_t) cos_thdid();
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

static inline microsec_t
time_to_microsec(const struct timespec *t)
{
	time_t seconds          = t->tv_sec;
	long nano_seconds       = t->tv_nsec;
	microsec_t microseconds = seconds * 1000000 + nano_seconds / 1000;

	return microseconds;
}

long
cos_nanosleep(const struct timespec *req, struct timespec *rem)
{
	time_t remaining_seconds;
	long remaining_nano_seconds;
	microsec_t remaining_microseconds;
	cycles_t wakeup_deadline, wakeup_time;
	int completed_successfully;

	if (!req) {
		errno = EFAULT;
		return -1;
	}

	/* FIXME: call scheduler component to finish this sleep */
	return 0;
}


long
cos_set_tid_address(int *tidptr)
{
	/* Just do nothing for now and hope that works */
	return 0;
}

/* struct user_desc {
 *     int  		  entry_number; // Ignore
 *     unsigned int  base_addr; // Pass to cos thread mod
 *     unsigned int  limit; // Ignore
 *     unsigned int  seg_32bit:1;
 *     unsigned int  contents:2;
 *     unsigned int  read_exec_only:1;
 *     unsigned int  limit_in_pages:1;
 *     unsigned int  seg_not_present:1;
 *     unsigned int  useable:1;
 * };
 */

void* backing_data[MAX_NUM_THREADS];

static void
setup_thread_area(void *thread, void* data)
{
	return ;
}

int
cos_set_thread_area(void* data)
{
	setup_thread_area(NULL, data);
	return 0;
}

int
cos_clone(int (*func)(void *), void *stack, int flags, void *arg, pid_t *ptid, void *tls, pid_t *ctid)
{
	if (!func) {
		errno = EINVAL;
		return -1;
	}
	/* FIXME: call scheduler component to finish this */
	//struct sl_thd * thd = sl_thd_alloc((cos_thd_fn_t)(void*)func, arg);
	if (tls) {
		setup_thread_area(NULL, tls);
	}
	return 0;
}

#define FUTEX_WAIT		0
#define FUTEX_WAKE		1
#define FUTEX_FD		2
#define FUTEX_REQUEUE		3
#define FUTEX_CMP_REQUEUE	4
#define FUTEX_WAKE_OP		5
#define FUTEX_LOCK_PI		6
#define FUTEX_UNLOCK_PI		7
#define FUTEX_TRYLOCK_PI	8
#define FUTEX_WAIT_BITSET	9

#define FUTEX_PRIVATE 128

#define FUTEX_CLOCK_REALTIME 256

struct futex_data
{
	int *uaddr;
	struct ps_list_head waiters;
};

struct futex_waiter
{
	thdid_t thdid;
	struct ps_list list;
};

#define FUTEX_COUNT 20
struct futex_data futexes[FUTEX_COUNT];

struct futex_data *
lookup_futex(int *uaddr)
{
	int last_free = -1;
	int i;

	for (i = 0; i < FUTEX_COUNT; i++) {
		if (futexes[i].uaddr == uaddr) {
			return &futexes[i];
		} else if (futexes[i].uaddr == 0) {
			last_free = i;
		}
	}
	if (last_free >= 0) {
		futexes[last_free] = (struct futex_data) {
			.uaddr = uaddr
		};
		ps_list_head_init(&futexes[last_free].waiters);
		return &futexes[last_free];
	}
	printc("Out of futex ids!");
	assert(0);
}

/* TODO: Cleanup empty futexes */

struct ps_lock futex_lock;

int
cos_futex(int *uaddr, int op, int val,
          const struct timespec *timeout, /* or: uint32_t val2 */
		  int *uaddr2, int val3)
{
	printc("futex not implemented\n");
	errno = ENOSYS;
	return -1;
}

void
syscall_emulation_setup(void)
{
	printc("syscall_emulation_setup\n");

	libc_syscall_override((cos_syscall_t)(void*)cos_write, __NR_write);
	libc_syscall_override((cos_syscall_t)(void*)cos_writev, __NR_writev);
	libc_syscall_override((cos_syscall_t)(void*)cos_ioctl, __NR_ioctl);
	libc_syscall_override((cos_syscall_t)(void*)cos_brk, __NR_brk);
	libc_syscall_override((cos_syscall_t)(void*)cos_munmap, __NR_munmap);
	libc_syscall_override((cos_syscall_t)(void*)cos_madvise, __NR_madvise);
	libc_syscall_override((cos_syscall_t)(void*)cos_mremap, __NR_mremap);

	libc_syscall_override((cos_syscall_t)(void*)cos_nanosleep, __NR_nanosleep);

	libc_syscall_override((cos_syscall_t)(void*)cos_rt_sigprocmask, __NR_rt_sigprocmask);
	libc_syscall_override((cos_syscall_t)(void*)cos_mprotect, __NR_mprotect);

	libc_syscall_override((cos_syscall_t)(void*)cos_gettid, __NR_gettid);
	libc_syscall_override((cos_syscall_t)(void*)cos_tkill, __NR_tkill);
	libc_syscall_override((cos_syscall_t)(void*)cos_mmap, __NR_mmap);
	libc_syscall_override((cos_syscall_t)(void*)cos_set_thread_area, __NR_set_thread_area);
#if defined(__x86__)
	libc_syscall_override((cos_syscall_t)(void*)cos_mmap, __NR_mmap2);
#endif
	libc_syscall_override((cos_syscall_t)(void*)cos_set_tid_address, __NR_set_tid_address);
	libc_syscall_override((cos_syscall_t)(void*)cos_clone, __NR_clone);
	libc_syscall_override((cos_syscall_t)(void*)cos_futex, __NR_futex);
}

/* TODO: init tls when creating components */
char tls_space[8192] = {0};
void tls_init()
{
	vaddr_t* tls_addr	= (vaddr_t *)&tls_space;
	*tls_addr 		= (vaddr_t)&tls_space;

	sched_set_tls((void*)tls_addr);
}

/* override musl-libc's init_tls() */
void __init_tls(size_t *auxv)
{
}

void
libc_initialization_handler()
{
	ps_lock_init(&stdout_lock);
	ps_lock_init(&futex_lock);
	printc("libc_init\n");
	libc_init();
	tls_init();
}
