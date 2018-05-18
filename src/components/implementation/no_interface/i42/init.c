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

struct sinv_async_info sinv_info;

extern void I42_AppMain();
extern void OS_IdleLoop();
extern unsigned int OS_TaskDelay(unsigned int millisecond);
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    printc("Starting i42 pre init\n");
    do_emulation_setup(cos_comp_info.cos_this_spd_id);

    acom_client_init(&sinv_info, RK_CLIENT(1));
    acom_client_thread_init(&sinv_info, cos_thdid(), 0, 0, RK_SKEY(1, 0));

    printc("Starting i42 main\n");
    I42_AppMain();
    printc("Ending i42 main\n");

    while(1) OS_IdleLoop();
}

/* HACK: THIS CODE SHOULD NOT BE HERE */
void
DOY2MD(long Year, long DayOfYear, long *Month, long *Day)
{
      long K;

      if (Year % 4 == 0) {
         K = 1;
      }
      else {
         K = 2;
      }

      if (DayOfYear < 32) {
         *Month = 1;
      }
      else {
         *Month = (long) (9.0*(K+DayOfYear)/275.0+0.98);
      }

      *Day = DayOfYear - 275*(*Month)/9 + K*(((*Month)+9)/12) + 30;
}

double
JDToAbsTime(double JD)
{
      return((JD-2451545.0)*86400.0);
}

double
YMDHMS2JD(long Year, long Month, long Day,
          long Hour, long Minute, double Second)
{
      long A,B;
      double JD;

      if (Month < 3) {
         Year--;
         Month+=12;
      }

      A = Year/100;
      B = 2 - A + A/4;

      JD = floor(365.25*(Year+4716)) + floor(30.6001*(Month+1))
           + Day + B - 1524.5;

      JD += ((double) Hour) / 24.0 +
            ((double) Minute) / 1440.0 +
            Second / 86400.0;

      return(JD);
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
    if (syscall_num == __NR_open) {
        return rk_inv_open_acom(&sinv_info, (const char *)a, (int)b, (mode_t) c, 0, 0);
    }

    if (syscall_num == __NR_read) {
        return rk_inv_read_acom(&sinv_info, (int)a, (void *)b, (size_t)c, 0, 0);
    }

    if (syscall_num == __NR_write) {
        return rk_inv_write_acom(&sinv_info, (int)a, (void *)b, (size_t)c, 0, 0);
    }

    if (syscall_num == __NR_clock_gettime) {
        microsec_t microseconds = ps_tsc() / cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
        time_t seconds = microseconds / 1000000;
        long rest = microseconds % 1000000;

        *((struct timespec *)b) = (struct timespec) {seconds, rest};
        return 0;
    }

    if (syscall_num == __NR_mmap || syscall_num == __NR_mmap2) {
        return (long)cos_mmap((void *)a, (size_t)b, (int)c, (int)d, (int)e, (off_t)f);
    }

    if (syscall_num == __NR_brk) {
        return 0;
    }

    if (syscall_num == __NR_socketcall) {
        return rk_inv_socketcall_acom(&sinv_info, (int)a, (void*)b, 0, 0);
    }

    printc("Unimplemented syscall number %d\n", syscall_num);
    assert(0);
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
