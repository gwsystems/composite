#include <math.h>
#include <syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <time.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ps.h>
#include <sinv_async.h>

#include <memmgr.h>
#include <rk_acom_lib.h>
#include <rk_types.h>

static struct sinv_async_info sinv_info;
static int rk_instance = 0, rk_thd_instance = 0;

/* call for each thread  */
void
posix_rk_thd_init(thdid_t tid)
{
	PRINTC("Enabing %d thread to talk to RK\n", tid);
	assert(rk_instance > 0);
	acom_client_thread_init(&sinv_info, tid, 0, 0, RK_SKEY(rk_instance, rk_thd_instance));
	rk_thd_instance++;
}

void
posix_rk_init(int instance)
{
	rk_instance = rk_args_instance();
	assert(rk_instance > 0);

	acom_client_init(&sinv_info, RK_CLIENT(rk_instance));
	posix_rk_thd_init(cos_thdid());
}

FILE *FileOpen(const char *Path, const char *File, const char *CtrlCode)
{
    assert(0);
    return NULL;
}

// HACK: The hack to end all hacks
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

    int pages;
    if (length % 4096) {
        pages = length / 4096 + 1;
    } else {
        pages = length / 4096;
    }

	addr = (void *)memmgr_heap_page_allocn(pages);
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

long
cos_syscall_handler(int syscall_num, long a, long b, long c, long d, long e, long f, long g)
{
	switch(syscall_num) {
	case __NR_open:
	{
		return rk_inv_open_acom(&sinv_info, (const char *)a, (int)b, (mode_t) c, 0, 0);
	}
	case __NR_read:
	{
		return rk_inv_read_acom(&sinv_info, (int)a, (void *)b, (size_t)c, 0, 0);
	}
	case __NR_write:
	{
		return rk_inv_write_acom(&sinv_info, (int)a, (void *)b, (size_t)c, 0, 0);
	}
	case __NR_writev:
	{
		return rk_inv_writev_acom(&sinv_info, (int)a, (const struct iovec *)b, (int)c, 0, 0);
	}
	case __NR_clock_gettime:
	{
		microsec_t microseconds = ps_tsc() / cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
		time_t seconds = microseconds / 1000000;
		long rest = microseconds % 1000000;

		*((struct timespec *)b) = (struct timespec) {seconds, rest};

		return 0;
	}
	case __NR_mmap:
	case __NR_mmap2:
	{
		return (long)cos_mmap((void *)a, (size_t)b, (int)c, (int)d, (int)e, (off_t)f);
	}
	case __NR_socketcall:
	{
		return rk_inv_socketcall_acom(&sinv_info, (int)a, (void*)b, 0, 0);
	}
	case __NR_brk:
	{
		return 0;
	}
	case __NR_fcntl64:
	{
		return rk_inv_fcntl_acom(&sinv_info, (int)a, (int)b, (void *)c, 0, 0);
	}
	case __NR_ioctl:
	default:
	{
		PRINTC("Unimplemented syscall number %d\n", syscall_num);
	}
	}

	return 0;
}

// Hack around thread local data
static int cancelstate = 0;

int
pthread_setcancelstate(int new, int *old)
{
    if (new > 2) return EINVAL;

    if (old) *old = cancelstate;
    cancelstate = new;
    return 0;
}
