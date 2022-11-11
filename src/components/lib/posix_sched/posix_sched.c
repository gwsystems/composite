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

static volatile int* null_ptr = NULL;
#define ABORT() do {int i = *null_ptr;} while(0)

int
cos_rt_sigprocmask(int how, void* set, void* oldset, size_t sigsetsize)
{
	/* Musl uses this at thread create time */
	printc("rt_sigprocmask not implemented\n");
	errno = ENOSYS;
	return -1;
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
static struct ps_lock futex_lock;

int
cos_futex(int *uaddr, int op, int val,
          const struct timespec *timeout, /* or: uint32_t val2 */
		  int *uaddr2, int val3)
{
	printc("futex not implemented\n");
	errno = ENOSYS;
	return -1;
}

int
cos_clock_gettime(clockid_t clock_id, struct timespec *ts)
{
	switch (clock_id)
	{
	case CLOCK_REALTIME:
		/* code */
		ts->tv_sec = 3600; //one hour after 1970-01-01, just a hack.
		break;
	
	default:
		break;
	}

	return 0;
}

/* TODO: init tls when creating components */
#define PER_THD_TLS_MEM_SZ 8192
char tls_space[PER_THD_TLS_MEM_SZ] = {0};
void tls_init()
{
	/* NOTE: GCC uses tls space similar to a stack, memory is accessed from high address to low address */
	vaddr_t* tls_addr	= (vaddr_t *)((char *)&tls_space + PER_THD_TLS_MEM_SZ - sizeof(vaddr_t));
	*tls_addr		= (vaddr_t)&tls_addr;

	sched_set_tls((void*)tls_addr);
}

void
libc_posixsched_initialization_handler()
{
	ps_lock_init(&futex_lock);
	libc_syscall_override((cos_syscall_t)(void*)cos_nanosleep, __NR_nanosleep);
	libc_syscall_override((cos_syscall_t)(void*)cos_rt_sigprocmask, __NR_rt_sigprocmask);
	libc_syscall_override((cos_syscall_t)(void*)cos_gettid, __NR_gettid);
	libc_syscall_override((cos_syscall_t)(void*)cos_tkill, __NR_tkill);
	libc_syscall_override((cos_syscall_t)(void*)cos_set_thread_area, __NR_set_thread_area);
	libc_syscall_override((cos_syscall_t)(void*)cos_set_tid_address, __NR_set_tid_address);
	libc_syscall_override((cos_syscall_t)(void*)cos_clone, __NR_clone);
	libc_syscall_override((cos_syscall_t)(void*)cos_futex, __NR_futex);
	libc_syscall_override((cos_syscall_t)(void*)cos_clock_gettime, __NR_clock_gettime);

	tls_init();
}
